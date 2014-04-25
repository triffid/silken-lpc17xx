#include "USBClient.h"

#include <cstdint>
#include <cstring>
#include <cstdio>

#include "USBFunction.h"

#define max(a, b) (((a) >= (b))?(a):(b))
#define min(a, b) (((a) >= (b))?(b):(a))

#include "platform_utils.h"

#define TRACE(...) printf(__VA_ARGS__)
// #define TRACE(...) do {} while (0)

USBClient::USBClient()
{
    usbdesc_device d = {
        DL_DEVICE,          // descriptor length
        DT_DEVICE,          // descriptor type
        USB_VERSION_2_0,    // usb version
        UC_MISC,            // device class
        SUBCLASS_IAD,       // device subclass
        PROTOCOL_IAD,       // device protocol
        EP0_MAX_PACKET,     // control endpoint max packet size
        0x1D50,             // vendor ID
        0x6015,             // product ID
        0x0100,             // release number
        0,                  // manufacturer string index
        0,                  // product string index
        0,                  // serial number string index
        1,                  // number of configurations
    };
    memcpy(&device_descriptor, &d, sizeof(device_descriptor));

    usbdesc_configuration c = {
        DL_CONFIGURATION,
        DT_CONFIGURATION,
        DL_CONFIGURATION,   // wTotalLength
        0,                  // bNumInterfaces
        1,                  // bConfigurationValue
        0,                  // iConfiguration
        CA_BUSPOWERED,      // bmAttributes
        500 mA,             // bMaxPower
    };
    memcpy(&configuration_descriptor, &c, sizeof(configuration_descriptor));

    usbdesc_language l = {
        DL_LANGUAGE,        // length
        DT_LANGUAGE,        // type
        {
            SL_USENGLISH        // language ID(s)
        }
    };
    memcpy(&language_descriptor, &l, sizeof(language_descriptor));
}

void USBClient::add_function(USBFunction* f)
{
    if (f)
    {
        functions.push_back(f);
        f->added(*this);
    }
}

void USBClient::remove_function(USBFunction* f)
{
    for (auto i = functions.begin(); i != functions.end(); i++)
    {
        if (*i == f)
        {
            functions.erase(i--);
            f->removed(*this);
            return;
        }
    }
}

void USBClient::usb_connect()
{
    for (USBFunction* f : functions)
        f->event_connect(*this);

    TRACE("USB connected\n");
}

void USBClient::usb_disconnect()
{
    for (USBFunction* f : functions)
        f->event_disconnect(*this);

    TRACE("USB disconnected\n");
}

void USBClient::usb_reset()
{
    for (USBFunction* f : functions)
        f->event_reset(*this);

    TRACE("USB reset\n");
}

void USBClient::usb_suspend()
{
    for (USBFunction* f : functions)
        f->event_suspend(*this);

    TRACE("USB suspend\n");
}

void USBClient::usb_wake()
{
    for (USBFunction* f : functions)
        f->event_resume(*this);

    TRACE("USB wake\n");
}

void USBClient::usb_setup()
{
    usb_setup_packet& setup = control.setup;

    if (read(EP0OUT, (uint8_t*) &setup) != 8)
        return;

    control.buffer = transfer_buffer;
    control.transfer_remaining = setup.wLength;
    control.zlp = ((setup.wLength == 0) && ((setup.bmRequestType & 1) == 0));

    TRACE("USB setup: type 0x%02X req 0x%02X value 0x%04X index 0x%04X data %ub\n", setup.bmRequestType, setup.bRequest, setup.wValue, setup.wIndex, setup.wLength);

    // Direction: 0 = OUT, 1 = IN
    uint8_t direction = setup.bmRequestType >> 7;
    // type: 0 = standard, 1 = class, 2 = vendor, 3 = reserved
    uint8_t type      = (setup.bmRequestType >> 5) & 3;
    // recipient: 0 = device, 1 = interface, 2 = endpoint, 3 = other, 4..31 = reserved
    uint8_t recipient = setup.bmRequestType & 31;

    switch(setup.bRequest)
    {
        case RQ_GET_STATUS:
            // TODO: check type and wIndex
            TRACE("USB get status\n");
            transfer_buffer[0] = 0;
            transfer_buffer[1] = 0;
            break;
        case RQ_CLEAR_FEATURE:
            break;
        case RQ_SET_FEATURE:
            break;
        case RQ_SET_ADDRESS:
            set_address(setup.wValue);
            TRACE("USB got address 0x%04X\n", setup.wValue);
            control.transfer_remaining = 0;
            control.zlp = true;
            break;
        case RQ_GET_DESCRIPTOR:
        {
            uint8_t type = setup.wValue >> 8;
            uint8_t index = setup.wValue & 0xFF;
            switch (type)
            {
                case DT_DEVICE:
                    TRACE("USB get Device descriptor\n");

                    control.buffer = (uint8_t*) &device_descriptor;
                    control.transfer_remaining = min(control.setup.wLength, DL_DEVICE);

                    break;
                case DT_CONFIGURATION:
                    TRACE("USB get Configuration descriptor\n");

                    control.transfer_remaining = configuration_descriptor.wTotalLength;

                    break;
                case DT_STRING:
                    TRACE("USB get String descriptor\n");
                    control.transfer_remaining = 0;
                    control.zlp = true;
                    // TODO
                    break;
                default:
                    TRACE("USB get unknown descriptor with type 0x%02X\n", type);
                    control.transfer_remaining = 0;
                    control.zlp = true;
                    break;
            }
            break;
        }
        case RQ_SET_DESCRIPTOR:
            TRACE("USB set descriptor: error unsupported\n");
            stall(EP0IN); // flag error
            break;
        case RQ_GET_CONFIGURATION:
            TRACE("USB get Configuration\n");
            transfer_buffer[0] = configuration_descriptor.bConfigurationValue;
            break;
        case RQ_SET_CONFIGURATION:
            TRACE("USB set Configuration: WE ARE GO\n");
            if (setup.wValue == 0)
                unconfigure();
            else
            {
                // TODO: something intelligent with wValue
                // TODO: "realise" relevant endpoints
                configure();
            }
            break;
        case RQ_GET_INTERFACE:
            TRACE("USB get interface\n");
            break;
        case RQ_SET_INTERFACE:
            TRACE("USB set interface\n");
            break;
        case RQ_SYNC_FRAME:
            break;
    }

    if ((control.transfer_remaining > 0) && (direction == 1))
    {
        // send first packet to get ball rolling. Invoke tx_complete to avoid code duplication
        usb_endpoint_tx_complete(EP0IN);
    }
}

void USBClient::usb_sof(uint16_t frame)
{
}

bool USBClient::usb_endpoint_tx_nak(uint8_t endpoint)
{
    TRACE("[EP%dIN NAK]", endpoint);
    return usb_endpoint_tx_complete(endpoint);
}

bool USBClient::usb_endpoint_tx_complete(uint8_t endpoint)
{
    if (endpoint == EP0IN)
    {
        if (control.transfer_remaining > 0)
        {
            TRACE("EP0IN/Control: bmRequestType=0x%02X bRequest=0x%02X wValue=0x%04X wIndex=%d wLength=%db / transfer_remaining=%db buffer=%p(%p)\n", control.setup.bmRequestType, control.setup.bRequest, control.setup.wValue, control.setup.wIndex, control.setup.wLength, control.transfer_remaining, control.buffer, transfer_buffer);

            uint8_t packet_length = 0;

            // detect RQ_GET_DESCRIPTOR(DT_CONFIGURATION)
            if ((control.setup.bmRequestType == 0x80) && (control.setup.bRequest = RQ_GET_DESCRIPTOR) && ((control.setup.wValue >> 8) == DT_CONFIGURATION))
            {
                if (control.transfer_remaining == configuration_descriptor.wTotalLength)
                {
                    // this is the first packet of the transfer
                    function_index = descriptor_index = byte_index = 0;
                    packet_length = min(DL_CONFIGURATION, EP0_MAX_PACKET);

                    memcpy(control.buffer, &configuration_descriptor, packet_length);
                }
                else if ((EP0_MAX_PACKET < DL_CONFIGURATION) && (control.transfer_remaining == (configuration_descriptor.wTotalLength - EP0_MAX_PACKET)))
                {
                    // this is the 2nd packet of a transfer with 8-byte packets
                    // since DL_CONFIGURATION > 8, we need to insert the remaining byte(s)
                    packet_length = DL_CONFIGURATION - EP0_MAX_PACKET;
                    memcpy(control.buffer, &((uint8_t*) &configuration_descriptor)[EP0_MAX_PACKET], packet_length);
                }

                /*
                 * gather various descriptors into transfer_buffer
                 * using state information stored in function_index, descriptor_index, byte_index
                 * and using packet_length as buffer pointer
                 */
                while ((packet_length < EP0_MAX_PACKET) && (packet_length < control.transfer_remaining))
                {
                    uint8_t rmn     = min(EP0_MAX_PACKET - packet_length, control.transfer_remaining - packet_length);

                    usbdesc_base* d = functions[function_index]->descriptors[descriptor_index];
                    uint8_t* db     = ((uint8_t*) d) + byte_index;

                    if ((d->bLength - byte_index) > rmn)
                    {
                        // current descriptor is larger than remaining buffer space
                        memcpy(&transfer_buffer[packet_length], db, rmn);

                        packet_length += rmn;
                        byte_index += rmn;
                    }
                    else
                    {
                        // current descriptor can fit entirely in the buffer. copy it, then find the next descriptor
                        memcpy(&transfer_buffer[packet_length], db, (d->bLength - byte_index) - rmn);

                        packet_length += (d->bLength - byte_index) - rmn;

                        // skip strings
                        do
                        {
                            descriptor_index++;
                        }
                            while ((functions[function_index]->descriptors[descriptor_index]->bDescType != DT_STRING) && (descriptor_index < functions[function_index]->descriptors.size()));

                        byte_index = 0;

                        // check if that was the last descriptor for this function
                        if (descriptor_index >= functions[function_index]->descriptors.size())
                        {
                            descriptor_index = 0;
                            function_index++;

                            // sanity check
                            if (function_index >= functions.size())
                                control.transfer_remaining = packet_length;
                        }
                    }
                }
                control.buffer = transfer_buffer;
            }
            else
            {
                packet_length = min(control.transfer_remaining, EP0_MAX_PACKET);
            }

            TRACE("[EP0IN:%d:", packet_length);
            for (int i = 0; i < packet_length; i++)
                TRACE("0x%02X ", control.buffer[i]);
            TRACE("]\n");

            write(endpoint, control.buffer, packet_length);

            control.zlp = (control.transfer_remaining == EP0_MAX_PACKET);

            control.transfer_remaining -= packet_length;
            control.buffer += packet_length;
        }
        else if (control.zlp)
        {
            TRACE("[EP0IN:0z]");
            write(endpoint, NULL, 0);
            control.zlp = false;
        }
        else
        {
            TRACE("[EP0IN complete]");
            control.transfer_remaining = 0;
        }
    }
    else
    {
        // TODO: invoke appropriate USB function handler
        TRACE("[EP%dIN:stall]", endpoint);
        stall(endpoint);
    }
    return true;
}

bool USBClient::usb_endpoint_rx_nak(uint8_t endpoint)
{
    TRACE("[EP%dOUT NAK]", endpoint);
    return true;
}

bool USBClient::usb_endpoint_rx(uint8_t endpoint)
{
    TRACE("[EP%dOUT", endpoint);

    if (endpoint == EP0OUT)
    {
        uint8_t i = read(endpoint, control.buffer);
        TRACE(":%d", i);
        if (i > 0)
        {
            control.transfer_remaining -= i;
            if (control.transfer_remaining == 0)
            {
                // TODO: something interesting with received data
            }
        }
    }
    else
    {
        // TODO: invoke appropriate USB function handler
    }

    TRACE("]");

    return true;
}

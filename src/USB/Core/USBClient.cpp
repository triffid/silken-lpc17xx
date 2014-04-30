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

    descriptors.push_back((usbdesc_base*) &device_descriptor);
    descriptors.push_back((usbdesc_base*) &configuration_descriptor);
    descriptors.push_back((usbdesc_base*) &language_descriptor);

    descriptor_index = 0;
    byte_index = 0;
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

int USBClient::addDescriptor(usbdesc_base* d)
{
    descriptors.push_back(d);

    configuration_descriptor.wTotalLength += d->bLength;

    return descriptors.size();
}

int USBClient::addInterface(usbdesc_interface* i)
{
    descriptors.push_back((usbdesc_base*) i);

    configuration_descriptor.wTotalLength += i->bLength;

    i->bInterfaceNumber = configuration_descriptor.bNumInterfaces;

    if (i->bAlternateSetting == 0)
        configuration_descriptor.bNumInterfaces++;

    return i->bInterfaceNumber;
}

int USBClient::addEndpoint(usbdesc_endpoint* e)
{
    int k = 0;
    usbdesc_interface* lastif = NULL;
    for (auto d : descriptors)
    {
        usbdesc_endpoint* de = (usbdesc_endpoint*) d;
        if (d->bDescType == DT_INTERFACE)
            lastif = (usbdesc_interface*) d;
        if ((d->bDescType == DT_ENDPOINT) && ((e->bmAttributes & 3) == (de->bmAttributes & 3)) && ((e->bEndpointAddress & 0x80) == (de->bEndpointAddress & 0x80)) && (lastif) && (lastif->bAlternateSetting == 0))
            k++;
    }

    e->bEndpointAddress = (index_to_endpoint(k, (USB_ENDPOINT_TYPES) (e->bmAttributes & 3)) & 0x7F) | (e->bEndpointAddress & 0x80);

    descriptors.push_back((usbdesc_base*) e);

    configuration_descriptor.wTotalLength += e->bLength;

    return k;
}

int USBClient::addString(const void* s)
{
    int k = 0;
    for (auto d : descriptors)
        if (d->bDescType == DT_STRING)
            k++;

    descriptors.push_back((usbdesc_base*) s);

    return k;
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
                    // default should handle this, but debug output is nice
                    TRACE("USB get Device descriptor\n");

                    control.buffer = (uint8_t*) &device_descriptor;
                    control.transfer_remaining = min(control.setup.wLength, DL_DEVICE);

                    break;
                case DT_CONFIGURATION:
                    TRACE("USB get Configuration descriptor\n");

                    /*
                     * set up state variables for descriptor gather routine below
                     */
                    control.transfer_remaining = min(control.setup.wLength, configuration_descriptor.wTotalLength);
                    byte_index = 0;
                    descriptor_index = 1;

                    break;
                default:
                {
                    TRACE("USB get descriptor type 0x%02X index %d\n", type, index);

                    uint8_t i = 0;
                    control.transfer_remaining = 0;
                    control.zlp = true;

                    for (auto descriptor : descriptors)
                    {
                        if (descriptor->bDescType == type)
                        {
                            if (i == index)
                            {
                                control.buffer = (uint8_t*) descriptor;
                                control.transfer_remaining = descriptor->bLength;
                                control.zlp = false;

                                TRACE("USB get descriptor: found!\n");

                                break;
                            }
                            i++;
                        }
                    }

                    if (control.transfer_remaining == 0)
                    {
                        TRACE("USB get descriptor: NOT found\n");
                    }
                    break;
                }
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
            if (setup.wValue == 0)
            {
                TRACE("USB unconfigure: STOP\n");
                unconfigure();
            }
            else
            {
                TRACE("USB set Configuration: WE ARE GO\n");
                // TODO: something intelligent with wValue
                // TODO: "realise" relevant endpoints
                configure();
            }
            break;
        case RQ_GET_INTERFACE:
            TRACE("USB get interface\n");
            // TODO: return relevant alternate interface
            break;
        case RQ_SET_INTERFACE:
            TRACE("USB set interface\n");
            // TODO: use relevant alternate interface
            break;
        case RQ_SYNC_FRAME:
            // TODO: handle whatever this is
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

            for (volatile uint32_t r = 0; r < 1UL<<15; r++);

            uint8_t packet_length = 0;

            // detect RQ_GET_DESCRIPTOR(DT_CONFIGURATION)
            if ((control.setup.bmRequestType == 0x80) && (control.setup.bRequest = RQ_GET_DESCRIPTOR) && ((control.setup.wValue >> 8) == DT_CONFIGURATION))
            {
                /*
                 * gather various descriptors into transfer_buffer
                 * using state information stored in descriptor_index, byte_index
                 * and using packet_length as buffer pointer
                 */
                while (packet_length < min(EP0_MAX_PACKET, control.transfer_remaining))
                {
                    uint8_t rmn     = min(EP0_MAX_PACKET, control.transfer_remaining) - packet_length;

                    usbdesc_base* d = descriptors[descriptor_index];
                    uint8_t* db     = ((uint8_t*) d) + byte_index;

                    if ((d->bLength - byte_index) > rmn)
                    {
                        TRACE("CONF DESC partial descriptor %d (type 0x%02X, @%db (of %d) + %db)\n", descriptor_index, d->bDescType, byte_index, d->bLength, rmn);
                        // current descriptor is larger than remaining buffer space
                        memcpy(&transfer_buffer[packet_length], db, rmn);

                        packet_length += rmn;
                        byte_index += rmn;
                    }
                    else
                    {
                        uint8_t c = min(rmn, d->bLength - byte_index);
                        TRACE("CONF DESC entire descriptor %d (type 0x%02X, @%db of %db. %db rmn, copy %db)\n", descriptor_index, d->bDescType, byte_index, d->bLength, rmn, c);

                        for (volatile uint32_t r = 1UL << 15; r; r--);

                        // current descriptor can fit entirely in the buffer. copy it, then find the next descriptor
                        memcpy(&transfer_buffer[packet_length], db, c);

                        packet_length += c;

                        // skip strings
                        byte_index = 0;
                        do
                        {
                            descriptor_index++;
                        }
                        while ((descriptor_index < descriptors.size()) && (descriptors[descriptor_index]->bDescType == DT_STRING));

                        // sanity-check for last descriptor
                        if (descriptor_index >= descriptors.size())
                            control.transfer_remaining = packet_length;
                    }

                    TRACE("CONF DESC %d of %db\n", packet_length, min(EP0_MAX_PACKET, control.transfer_remaining));
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

#include "USBClient.h"

#include "USBFunction.h"

struct usb_setup_packet __attribute__ ((packed))
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

USBClient::USBClient()
{
}

void USBClient::connect()
{
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
            return;
        }
    }
}

void usb_connect()
{
}

void usb_disconnect()
{
}

void usb_reset()
{
}

void usb_suspend()
{
}

void usb_wake()
{
}

void usb_setup()
{
    usb_setup_packet setup;
    uint8_t r = read(EP0OUT, (uint8_t*) &setup);
    if (r != 8)
        return;

    // Direction: 0 = OUT, 1 = IN
    uint8_t direction = setup.bmRequestType >> 7;
    // type: 0 = standard, 1 = class, 2 = vendor, 3 = reserved
    uint8_t type      = (setup.bmRequestType >> 5) & 3;
    // recipient: 0 = device, 1 = interface, 2 = endpoint, 3 = other, 4..31 = reserved
    uint8_t recipient = setup.bmRequestType & 31;

    if (type == 0)
    {
        // standard request
        switch (recipient)
        {
            case 0: // device
            {
                switch(setup.bRequest)
                {
                    case 0x00: // GET_STATUS
                    case 0x01: // CLEAR_FEATURE
                    case 0x03: // SET_FEATURE
                    case 0x05: // SET_ADDRESS
                    case 0x06: // GET_DESCRIPTOR
                    case 0x07: // SET_DESCRIPTOR
                    case 0x08: // GET_CONFIGURATION
                    case 0x09: // SET_CONFIGURATION
                        break;
                }
                break;
            };
            case 1: // interface
            {
                switch (setup.bRequest)
                {
                    case 0x00: // GET_STATUS
                    case 0x01: // CLEAR_FEATURE
                    case 0x03: // SET_FEATURE
                    case 0x0A: // GET_INTERFACE
                    case 0x11: // SET_INTERFACE
                        break;
                }
                break;
            };
            case 2: // endpoint
            {
                switch (setup.bRequest)
                {
                    case 0x00: // GET_STATUS
                    case 0x01: // CLEAR_FEATURE
                    case 0x03: // SET_FEATURE
                    case 0x12: // SYNCH_FRAME
                        break;
                }
                break;
            }
        }
    }
    else if (type == 1)
    {
        // class request
    }
}

void usb_sof(uint16_t frame)
{
}

bool usb_endpoint_nak(uint8_t endpoint)
{
}

bool usb_endpoint_tx_complete(uint8_t endpoint)
{
}

bool usb_endpoint_rx(uint8_t endpoint)
{
}

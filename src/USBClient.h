#ifndef _USBCLIENT_H
#define _USBCLIENT_H

#include <cstdint>
#include <vector>

#include "USBhw.h"

class USBFunction;

typedef struct
{
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint8_t  data[];
}
ControlPacket_t;

class USBClient : public USBhw
{
public:
    USBClient();

    void connect();

    void add_function(USBFunction*);
    void remove_function(USBFunction*);

    /*
     * implementation of USBhw callbacks
     */
    void usb_connect();
    void usb_disconnect();

    void usb_reset();

    void usb_suspend();
    void usb_wake();

    void usb_sof(uint16_t frame);

    void usb_setup();

    bool usb_endpoint_nak(uint8_t endpoint);
    bool usb_endpoint_tx_complete(uint8_t endpoint);

    bool usb_endpoint_rx(uint8_t endpoint);

protected:
    std::vector<USBFunction*> functions;
};

#endif /* _USBCLIENT_H */

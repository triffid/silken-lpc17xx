#ifndef _USBHW_H
#define _USBHW_H

#include <cstdint>

// control endpoints
#define EP0OUT 0
#define EP0IN  0x80

#define EP0_MAX_PACKET 64

typedef enum {
    CONTROL,
    INTERRUPT,
    BULK,
    ISOCHRONOUS
} USB_ENDPOINT_TYPES;

struct USBhw_state;

class USBhw;
class USBhw
{
public:
    USBhw();

    /*
     * USB stack must implement these callbacks
     */
    virtual void usb_connect() = 0;
    virtual void usb_disconnect() = 0;

    virtual void usb_reset() = 0;

    virtual void usb_suspend() = 0;
    virtual void usb_wake() = 0;

    virtual void usb_setup() = 0;

    virtual void usb_sof(uint16_t frame) = 0;

    virtual bool usb_endpoint_tx_nak(uint8_t endpoint) = 0;
    virtual bool usb_endpoint_tx_complete(uint8_t endpoint) = 0;

    virtual bool usb_endpoint_rx_nak(uint8_t endpoint) = 0;
    virtual bool usb_endpoint_rx(uint8_t endpoint) = 0;

    /*
     * USB hardware methods
     */

    void    connect();
    void    disconnect();

    void    set_address(uint16_t);
    void    configure();
    void    unconfigure();

    void    stall(uint8_t endpoint);
    void    unstall(uint8_t endpoint);

    bool    begin_read( uint8_t endpoint, uint8_t* buffer);
    bool    begin_write(uint8_t endpoint, uint8_t* buffer);

    uint8_t read( uint8_t endpoint, uint8_t* buffer);
    uint8_t write(uint8_t endpoint, uint8_t* buffer, uint8_t length);

    uint8_t index_to_endpoint(uint8_t index, USB_ENDPOINT_TYPES type);

    void    usbisr(void);

    static  USBhw* instance;

protected:
    struct USBhw_state* state;
};

#endif /* _USBHW_H */

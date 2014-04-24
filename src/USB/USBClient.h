#ifndef _USBCLIENT_H
#define _USBCLIENT_H

#include <cstdint>
#include <vector>

#include "USBhw.h"

#include "descriptor.h"

class USBFunction;

typedef struct __attribute__ ((packed))
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet;

typedef struct
{
    usb_setup_packet setup;

    uint16_t transfer_remaining;
    uint8_t* buffer;
    bool     zlp;
} usb_control_transfer;

class USBClient : public USBhw
{
public:
    USBClient();

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

    bool usb_endpoint_tx_nak(uint8_t endpoint);
    bool usb_endpoint_tx_complete(uint8_t endpoint);

    bool usb_endpoint_rx_nak(uint8_t endpoint);
    bool usb_endpoint_rx(uint8_t endpoint);

    /*
     * descriptor insertion functions
     */
    // generic descriptor
    int addDescriptor(usbdesc_base*);
    int addDescriptor(void*);

    // interface, endpoint, string descriptors for which we require a specific index
    int addInterface(usbdesc_interface*);
    int addEndpoint(usbdesc_endpoint*);
    int addString(const void*);

    void removeDescriptor(usbdesc_base*);
    void removeInterface(usbdesc_interface*);
    void removeEndpoint(usbdesc_endpoint*);
    void removeString(const void*);

    /*
     * usb required descriptors
     */
    usbdesc_device        device_descriptor;
    usbdesc_configuration configuration_descriptor;
    usbdesc_language      language_descriptor;

protected:
    std::vector<USBFunction*> functions;

    usb_control_transfer control;
    uint8_t transfer_buffer[EP0_MAX_PACKET];

    /*
     * for gathering descriptors on REQ_GET_DESCRIPTOR(DT_CONFIGURATION)
     */
    struct {
        uint16_t function_index;
        uint16_t descriptor_index;
        uint16_t byte_index;
    };
};

#endif /* _USBCLIENT_H */

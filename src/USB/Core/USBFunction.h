#ifndef _USBFUNCTION_H
#define _USBFUNCTION_H

#include <cstdint>

#include <vector>

#include "USBClient.h"

class USBFunction
{
    friend class USBClient;

public:
    USBFunction(){};

    std::vector<usbdesc_base*> descriptors;

protected:
    virtual void added(USBClient&)   {};
    virtual void removed(USBClient&) {};

    /*
     * USB Events
     */
    virtual void event_connect(   USBClient&) {};
    virtual void event_disconnect(USBClient&) {};

    virtual void event_reset(     USBClient&) {};

    virtual void event_suspend(   USBClient&) {};
    virtual void event_resume(    USBClient&) {};

    virtual void event_control(   USBClient&, usb_control_transfer& packet) {};

    virtual void event_recvdata(  USBClient&, uint8_t endpoint_index, uint8_t  length, void* data) {};
    virtual void event_polldata(  USBClient&, uint8_t endpoint_index, uint8_t* length, void* data) {};
};

#endif /* _USBFUNCTION_H */

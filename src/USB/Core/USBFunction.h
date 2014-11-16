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

protected:
    /*
     * module connectivity
     */
    virtual void added(  USBClient&) {};
    virtual void removed(USBClient&) {};

    // is this our descriptor?
    virtual bool check_owner(usbdesc_base*) { return false; };

    /*
     * USB Events
     */
    virtual void event_connect(   USBClient&) {};
    virtual void event_disconnect(USBClient&) {};

    virtual void event_reset(     USBClient&) {};

    virtual void event_suspend(   USBClient&) {};
    virtual void event_resume(    USBClient&) {};

    virtual void event_sof(       USBClient&, uint16_t frame) {};

    virtual bool event_control(   USBClient&, usb_control_transfer& packet) { return false; };

    virtual bool event_recvdata(  USBClient&, uint8_t endpoint_index, uint8_t  length, void* data) { return false; };
    virtual bool event_polldata(  USBClient&, uint8_t endpoint_index, uint8_t* length, void* data) { return false; };
};

#endif /* _USBFUNCTION_H */

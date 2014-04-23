#ifndef _USBFUNCTION_H
#define _USBFUNCTION_H

#include "USBClient.h"

#include <cstdint>

class USBFunction
{
    friend class USBClient;

public:
    USBFunction();

    virtual void* get_descriptors(int index);

protected:
    virtual void added(USBClient&);

    /*
     * USB Events
     */
    virtual void event_connect(   USBClient&);
    virtual void event_disconnect(USBClient&);

    virtual void event_suspend(   USBClient&);
    virtual void event_resume(    USBClient&);

    virtual void event_control(   USBClient&, ControlPacket_t& packet);

    virtual void event_recvdata(  USBClient&, uint8_t endpoint_index, uint8_t  length, void* data);
    virtual void event_polldata(  USBClient&, uint8_t endpoint_index, uint8_t* length, void* data);
};

#endif /* _USBFUNCTION_H */

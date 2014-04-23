#include "USBFunction.h"

static uint16_t dummy = 0;

USBFunction::USBFunction()
{
}

void* get_descriptors(int index)
{
    return &dummy;
}

void added(USBClient& c)
{
}

void event_connect(   USBClient&)
{
}

void event_disconnect(USBClient&)
{
}

void event_suspend(   USBClient&)
{
}

void event_resume(    USBClient&)
{
}

void event_control(   USBClient&, ControlPacket_t& packet)
{
}

void event_recvdata(  USBClient&, uint8_t endpoint_index, uint8_t  length, void* data)
{
}

void event_polldata(  USBClient&, uint8_t endpoint_index, uint8_t* length, void* data)
{
}

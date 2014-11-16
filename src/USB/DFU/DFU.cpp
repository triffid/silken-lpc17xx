#include "DFU.h"

#define TRACE(...) printf(__VA_ARGS__)
// #define TRACE(...) do {} while (0)

#include "lpc17xx_wdt.h"

#include <stdio.h>
#include <mri.h>
#include <cstring>

DFU::DFU()
{
    dfu_interface = {
        DL_INTERFACE,
        DT_INTERFACE,
        0,                          // bInterfaceNumber: filled out during addInterface()
        0,                          // bAlternateSetting
        0,                          // bNumEndpoints
        DFU_INTERFACE_CLASS,        // bInterfaceClass
        DFU_INTERFACE_SUBCLASS,     // bInterfaceSubClass
        DFU_INTERFACE_PROTOCOL_RUNTIME, // bInterfaceProtocol
        0,                          // iInterface
    };

    dfu_descriptor = {
        DL_DFU_FUNCTIONAL_DESCRIPTOR,
        DT_DFU_FUNCTIONAL_DESCRIPTOR,
        DFU_BMATTRIBUTES_WILLDETACH | DFU_BMATTRIBUTES_CANDOWNLOAD | DFU_BMATTRIBUTES_CANUPLOAD,
        2000,               // wDetachTimeout
        512,                // wTransferSize
        DFU_VERSION_1_1,    // bcdDFUVersion
    };

    usbdesc_string_l(23) s = usbstring("Device Firmware Update");
    memcpy(&dfu_string, &s, sizeof(dfu_string));

    prep_for_detach = 0;
}

void DFU::added(USBClient& u)
{
    usb = &u;
    TRACE("DFU: added! Got USB=%p\n", usb);
    usb->addInterface(&dfu_interface);
    usb->addDescriptor((usbdesc_base*) &dfu_descriptor);
    dfu_interface.iInterface = usb->addString(&dfu_string);
}

bool DFU::check_owner(usbdesc_base* desc)
{
    void* d = (void*) desc;

    if (d == &dfu_interface)  return true;
    if (d == &dfu_descriptor) return true;
    if (d == &dfu_string)     return true;

    return false;
}

bool DFU::event_control(USBClient& u, usb_control_transfer& control)
{
    TRACE("Got DFU Control Request: type 0x%02X request %d length %d\n", control.setup.bmRequestType, control.setup.bRequest, control.setup.wLength);

    if (((control.setup.bmRequestType >> 5) & 3) == 1)
    {
        // CLASS request
        if ((control.setup.bmRequestType & 0x1F) == 1)
        {
            // INTERFACE target (presumably ours, or USBClient wouldn't call this function)
            switch (control.setup.bRequest)
            {
                case  DFU_DETACH:
                {
                    TRACE("DFU: detach\n");
                    prep_for_detach = 128;
                    WDT_Init(WDT_CLKSRC_IRC, WDT_MODE_RESET);
                    WDT_Start(250000); // 0.25 seconds
                    return true;
                }
                case DFU_GETSTATUS:
                {
                    dfu_status = {
                        0,      // status OK
                        500,    // bwPollTimeout
                        0,      // state appIdle
                        0       // iString
                    };
                    control.buffer = (uint8_t *) &dfu_status;
                    control.transfer_remaining = sizeof(dfu_status);
                    return true;
                }
                case DFU_CLRSTATUS:
                {
                    return true;
                }
                case DFU_GETSTATE:
                {
                    dfu_status.bState = 0; // appIdle
                    control.buffer    = &dfu_status.bState;
                    control.transfer_remaining = sizeof(dfu_status.bState);
                    return true;
                }
                case DFU_ABORT:
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void DFU::event_reset(USBClient& u)
{
    TRACE("DFU: got reset event\n");
    if (prep_for_detach)
    {
        usb->disconnect();
        for (;;);
    }
}

// bool DFU::USBEvent_Request(CONTROL_TRANSFER &control)
// {
//     printf("Got DFU Control Request: %d length %d\n", control.setup.bRequest, control.setup.wLength);
//     switch (control.setup.bRequest)
//     {
//         case  DFU_DETACH:
//         {
//             prep_for_detach = 128;
//             WDT_Init(WDT_CLKSRC_IRC, WDT_MODE_RESET);
//             WDT_Start(250000); // 0.25 seconds
//             return true;
//         }
//         case DFU_GETSTATUS:
//         {
//             dfu_status = {
//                 0,      // status OK
//                 500,    // bwPollTimeout
//                 0,      // state appIdle
//                 0       // iString
//             };
//             control.direction = DEVICE_TO_HOST;
//             control.ptr = (uint8_t *) &dfu_status;
//             control.remaining = sizeof(dfu_status);
//             return true;
//         }
//         case DFU_CLRSTATUS:
//         {
//             return true;
//         }
//         case DFU_GETSTATE:
//         {
//             dfu_status.bState = 0; // appIdle
//             control.direction = DEVICE_TO_HOST;
//             control.ptr = (uint8_t *) &dfu_status.bState;
//             control.remaining = 1;
//             return true;
//         }
//         case DFU_ABORT:
//         {
//             return true;
//         }
//     }
//     return false;
// }

// bool DFU::USBEvent_RequestComplete(CONTROL_TRANSFER &control, uint8_t *buf, uint32_t length)
// {
//     return false;
// }

void DFU::on_module_loaded()
{
//     register_for_event(ON_IDLE);
}

void DFU::on_idle()
{
    if (prep_for_detach)
    {
        prep_for_detach--;
        if (prep_for_detach == 0)
        {
            usb->disconnect();
            for (;;);
        }
    }
}

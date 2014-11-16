#ifndef _DFU_H
#define _DFU_H

#include "USBFunction.h"

#include <stdint.h>

#define REQ_DFU_DETACH      0x0
#define REQ_DFU_DNLOAD      0x1
#define REQ_DFU_UPLOAD      0x2
#define REQ_DFU_GETSTATUS   0x3
#define REQ_DFU_CLRSTATUS   0x4
#define REQ_DFU_GETSTATE    0x5
#define REQ_DFU_ABORT       0x6

#define DL_DFU_FUNCTIONAL_DESCRIPTOR    0x09
#define DT_DFU_FUNCTIONAL_DESCRIPTOR    0x21

#define DFU_VERSION_1_1                 0x0110

#define DFU_INTERFACE_CLASS             0xFE
#define DFU_INTERFACE_SUBCLASS          0x01
#define DFU_INTERFACE_PROTOCOL_RUNTIME  0x01
#define DFU_INTERFACE_PROTOCOL_DFUMODE  0x02

#define DFU_BMATTRIBUTES_WILLDETACH         (1<<3)
#define DFU_BMATTRIBUTES_MANIFEST_TOLERANT  (1<<2)  /* device keeps talking while flashing? */
#define DFU_BMATTRIBUTES_CANUPLOAD          (1<<1)
#define DFU_BMATTRIBUTES_CANDOWNLOAD        (1<<0)

#define DFU_DETACH      0
#define DFU_DNLOAD      1
#define DFU_UPLOAD      2
#define DFU_GETSTATUS   3
#define DFU_CLRSTATUS   4
#define DFU_GETSTATE    5
#define DFU_ABORT       6

class DFU;

typedef struct PACKED
{
    uint8_t     bLength;
    uint8_t     bDescriptorType;
    uint8_t     bmAttributes;
    uint16_t    wDetachTimeout;
    uint16_t    wTransferSize;
    uint16_t    bcdDFUVersion;
} DFU_functional_descriptor;

typedef struct PACKED
{
    uint8_t     bStatus;
    uint32_t    bwPollTimeout:24;
    uint8_t     bState;
    uint8_t     iString;
} DFU_Status_Response;

class USBClient;
class DFU : public USBFunction
{
public:
    DFU();

//     bool USBEvent_Request(CONTROL_TRANSFER&);
//     bool USBEvent_RequestComplete(CONTROL_TRANSFER&, uint8_t *buf, uint32_t length);

    /*
     * USBFunction implementation
     */
    void added(USBClient&);

    bool check_owner(usbdesc_base*);

    bool event_control(   USBClient&, usb_control_transfer& packet);

    void event_reset(     USBClient&);

    // end USBFunction

    void on_module_loaded(void);
    void on_idle(void);

protected:
    USBClient*                  usb;
    DFU_functional_descriptor   dfu_descriptor;
    usbdesc_interface           dfu_interface;

    usbdesc_string_l(12)        dfu_string;

    DFU_Status_Response         dfu_status;

    uint8_t                     prep_for_detach;
};

#endif /* _DFU_H */

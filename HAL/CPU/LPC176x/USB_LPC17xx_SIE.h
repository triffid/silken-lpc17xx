#ifndef _USB_LPC17XX_SIE_H
#define _USB_LPC17XX_SIE_H

#include <cstdint>

// Convert physical endpoint number to register bit
#define EP(endpoint) (1UL<<endpoint)

// Convert logical endpoint number to hardware index
#define EP2IDX(bEP)       ((((bEP)&0xF)<<1)|(((bEP)&0x80)>>7))
// Convert hardware index to logical endpoint number
#define IDX2EP(idx)       ((((idx)<<7)&0x80)|(((idx)>>1)&0xF))

// Get endpoint direction
#define IN_EP(endpoint)     ((endpoint) & 1U ? true : false)
#define OUT_EP(endpoint)    ((endpoint) & 1U ? false : true)

#define  IN_BEP(endpoint)    ((endpoint) & 0x80 ? true : false)
#define OUT_BEP(endpoint)    ((endpoint) & 0x80 ? false : true)

#define ISOCHRONOUS_ENDPOINTS ((1UL << 3) | (1UL << 6) | (1UL << 9) | (1UL << 12))
#define IS_ISOCHRONOUS(bEP) ((1UL << (bEP & 0x0F)) & ISOCHRONOUS_ENDPOINTS)

// control endpoints
#define EP0OUT 0
#define EP0IN  1

// USB Control register
#define RD_EN (1<<0)
#define WR_EN (1<<1)
#define LOG_ENDPOINT(endpoint) ((endpoint>>1)<<2)

// USB Device Interupt registers
#define FRAME      (1UL<<0)
#define EP_FAST    (1UL<<1)
#define EP_SLOW    (1UL<<2)
#define DEV_STAT   (1UL<<3)
#define CCEMPTY    (1UL<<4)
#define CDFULL     (1UL<<5)
#define RxENDPKT   (1UL<<6)
#define TxENDPKT   (1UL<<7)
#define EP_RLZED   (1UL<<8)
#define ERR_INT    (1UL<<9)

// Serial Interface Engine (SIE)
#define SIE_WRITE   (0x01)
#define SIE_READ    (0x02)
#define SIE_COMMAND (0x05)
#define SIE_CMD_CODE(phase, data) ((phase<<8)|(data<<16))

// SIE Command codes
#define SIE_CMD_SET_ADDRESS        (0xD0)
#define SIE_CMD_CONFIGURE_DEVICE   (0xD8)
#define SIE_CMD_SET_MODE           (0xF3)
#define SIE_CMD_READ_FRAME_NUMBER  (0xF5)
#define SIE_CMD_READ_TEST_REGISTER (0xFD)
#define SIE_CMD_SET_DEVICE_STATUS  (0xFE)
#define SIE_CMD_GET_DEVICE_STATUS  (0xFE)
#define SIE_CMD_GET_ERROR_CODE     (0xFF)
#define SIE_CMD_READ_ERROR_STATUS  (0xFB)

#define SIE_CMD_SELECT_ENDPOINT(endpoint)                 (0x00+endpoint)
#define SIE_CMD_SELECT_ENDPOINT_CLEAR_INTERRUPT(endpoint) (0x40+endpoint)
#define SIE_CMD_SET_ENDPOINT_STATUS(endpoint)             (0x40+endpoint)

#define SIE_CMD_CLEAR_BUFFER    (0xF2)
#define SIE_CMD_VALIDATE_BUFFER (0xFA)

// SIE Device Set Address register
#define SIE_DSA_DEV_EN  (1<<7)

// SIE Configue Device register
#define SIE_CONF_DEVICE (1<<0)

// Select Endpoint register
#define SIE_SE_FE       (1<<0)
#define SIE_SE_ST       (1<<1)
#define SIE_SE_STP      (1<<2)
#define SIE_SE_PO       (1<<3)
#define SIE_SE_EPN      (1<<4)
#define SIE_SE_B_1_FULL (1<<5)
#define SIE_SE_B_2_FULL (1<<6)

// Set Endpoint Status command
#define SIE_SES_ST      (1<<0)
#define SIE_SES_DA      (1<<5)
#define SIE_SES_RF_MO   (1<<6)
#define SIE_SES_CND_ST  (1<<7)

// endpoint modes
#define SIE_MODE_AP_CLK     (1<<0)
#define SIE_MODE_INAK_CI    (1<<1)
#define SIE_MODE_INAK_CO    (1<<2)
#define SIE_MODE_INAK_II    (1<<3)
#define SIE_MODE_INAK_IO    (1<<4)
#define SIE_MODE_INAK_BI    (1<<5)
#define SIE_MODE_INAK_BO    (1<<6)

// SIE Device Status register
#define SIE_DS_CON    (1<<0)
#define SIE_DS_CON_CH (1<<1)
#define SIE_DS_SUS    (1<<2)
#define SIE_DS_SUS_CH (1<<3)
#define SIE_DS_RST    (1<<4)

/* Select Endpoint command read bits */
#define EPSTAT_FE                   (1<<0)
#define EPSTAT_ST                   (1<<1)
#define EPSTAT_STP                  (1<<2)
#define EPSTAT_PO                   (1<<3)
#define EPSTAT_EPN                  (1<<4)
#define EPSTAT_B1FULL               (1<<5)
#define EPSTAT_B2FULL               (1<<6)

extern volatile uint32_t USBEpIntEn;
extern uint8_t can_transfer[32];

void     SIECommand(uint32_t command);

void     SIEWriteData(uint8_t data);
uint8_t  SIEReadData(uint32_t command);

void     SIEsetDeviceStatus(uint8_t status);
uint8_t  SIEgetDeviceStatus(void);

void     SIEsetMode(uint8_t mode);

void     SIEsetAddress(uint8_t address);

uint8_t  SIEselectEndpoint(uint8_t bEP);

uint8_t  SIEclearBuffer(void);
void     SIEvalidateBuffer(void);

void     SIEsetEndpointStatus(uint8_t bEP, uint8_t status);

uint16_t SIEgetFrameNumber(void);

void     SIEconfigureDevice(void);
void     SIEunconfigureDevice(void);

void     SIEconnect(void);
void     SIEdisconnect(void);

uint8_t  selectEndpointClearInterrupt(uint8_t bEP);

void     enableEndpointEvent(uint8_t bEP);
void     disableEndpointEvent(uint8_t bEP);

bool     realiseEndpoint(uint8_t bEP, uint32_t maxPacket, uint32_t flags);

void     stallEndpoint(uint8_t bEP);
void     unstallEndpoint(uint8_t bEP);
bool     getEndpointStallState(uint8_t bEP);

bool     endpointSetInterrupt(uint8_t bEP, bool enabled);
bool     endpointGetInterrupt(uint8_t bEP);
void     endpointTriggerInterrupt(uint8_t bEP);

void     _ignore(uint32_t);

#endif /* _USB_LPC17XX_SIE_H */

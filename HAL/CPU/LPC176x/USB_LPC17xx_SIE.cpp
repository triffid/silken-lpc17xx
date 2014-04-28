#include "USB_LPC17xx_SIE.h"

#include "LPC17xx.h"

#ifndef setled
    #define setled(a, b) do {} while (0)
#endif

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

// Set Endpoint Status command
#define SIE_SES_ST      (1<<0)
#define SIE_SES_DA      (1<<5)
#define SIE_SES_RF_MO   (1<<6)
#define SIE_SES_CND_ST  (1<<7)

#define EP(a)         (1UL << (a))


volatile uint32_t USBEpIntEn;
uint32_t endpointStallState;
uint8_t can_transfer[32];


void SIECommand(uint32_t command) {
    // The command phase of a SIE transaction
    LPC_USB->USBDevIntClr = CCEMPTY;
    LPC_USB->USBCmdCode = SIE_CMD_CODE(SIE_COMMAND, command);
    setled(4, 1); while (!(LPC_USB->USBDevIntSt & CCEMPTY)); setled(4, 0);
}

void SIEWriteData(uint8_t data) {
    // The data write phase of a SIE transaction
    LPC_USB->USBDevIntClr = CCEMPTY;
    LPC_USB->USBCmdCode = SIE_CMD_CODE(SIE_WRITE, data);
    setled(4, 1); while (!(LPC_USB->USBDevIntSt & CCEMPTY)); setled(4, 0);
}

uint8_t SIEReadData(uint32_t command) {
    // The data read phase of a SIE transaction
    LPC_USB->USBDevIntClr = CDFULL;
    LPC_USB->USBCmdCode = SIE_CMD_CODE(SIE_READ, command);
    setled(4, 1); while (!(LPC_USB->USBDevIntSt & CDFULL)); setled(4, 0);
    return (uint8_t)LPC_USB->USBCmdData;
}

void SIEsetDeviceStatus(uint8_t status) {
    // Write SIE device status register
    SIECommand(SIE_CMD_SET_DEVICE_STATUS);
    SIEWriteData(status);
}

uint8_t SIEgetDeviceStatus(void) {
    // Read SIE device status register
    SIECommand(SIE_CMD_GET_DEVICE_STATUS);
    return SIEReadData(SIE_CMD_GET_DEVICE_STATUS);
}

uint8_t SIEgetError(void)
{
    SIECommand(SIE_CMD_READ_ERROR_STATUS);
    return SIEReadData(SIE_CMD_READ_ERROR_STATUS);
}

void SIEsetMode(uint8_t mode) {
    SIECommand(SIE_CMD_SET_MODE);
    SIEWriteData(mode);
}

void SIEsetAddress(uint8_t address) {
    // Write SIE device address register
    SIECommand(SIE_CMD_SET_ADDRESS);
    SIEWriteData((address & 0x7f) | SIE_DSA_DEV_EN);
}

uint8_t SIEselectEndpoint(uint8_t bEP) {
    uint8_t endpoint = EP2IDX(bEP);

    // SIE select endpoint command
    SIECommand(SIE_CMD_SELECT_ENDPOINT(endpoint));
    return SIEReadData(SIE_CMD_SELECT_ENDPOINT(endpoint));
}

uint8_t SIEclearBuffer(void) {
    // SIE clear buffer command
    SIECommand(SIE_CMD_CLEAR_BUFFER);
    return SIEReadData(SIE_CMD_CLEAR_BUFFER);
}

void SIEvalidateBuffer(void) {
    // SIE validate buffer command
    SIECommand(SIE_CMD_VALIDATE_BUFFER);
}

void SIEsetEndpointStatus(uint8_t bEP, uint8_t status) {
    uint8_t endpoint = EP2IDX(bEP);

    // SIE set endpoint status command
    SIECommand(SIE_CMD_SET_ENDPOINT_STATUS(endpoint));
    SIEWriteData(status);
}

uint16_t SIEgetFrameNumber(void) {
    // Read current frame number
    uint16_t lowByte;
    uint16_t highByte;

    SIECommand(SIE_CMD_READ_FRAME_NUMBER);
    lowByte = SIEReadData(SIE_CMD_READ_FRAME_NUMBER);
    highByte = SIEReadData(SIE_CMD_READ_FRAME_NUMBER);

    return (highByte << 8) | lowByte;
}

void SIEconfigureDevice(void) {
    // SIE Configure device command
    SIECommand(SIE_CMD_CONFIGURE_DEVICE);
    SIEWriteData(SIE_CONF_DEVICE);
}

void SIEunconfigureDevice(void) {
    // SIE Configure device command
    SIECommand(SIE_CMD_CONFIGURE_DEVICE);
    SIEWriteData(0);
}

void SIEconnect(void) {
    // Connect USB device
    uint8_t status;

    status = SIEgetDeviceStatus();
    SIEsetDeviceStatus(status | SIE_DS_CON);
}


void SIEdisconnect(void) {
    // Disconnect USB device
    uint8_t status;

    status = SIEgetDeviceStatus();
    SIEsetDeviceStatus(status & ~SIE_DS_CON);
}


uint8_t selectEndpointClearInterrupt(uint8_t bEP) {
    uint8_t endpoint = EP2IDX(bEP);

    // Implemented using using EP_INT_CL
    LPC_USB->USBEpIntClr = EP(endpoint);
    setled(4, 1); while (!(LPC_USB->USBDevIntSt & CDFULL)); setled(4, 0);
    return (uint8_t)LPC_USB->USBCmdData;
}

void enableEndpointEvent(uint8_t bEP) {
    uint8_t endpoint = EP2IDX(bEP);

    // Enable an endpoint interrupt
    USBEpIntEn |= EP(endpoint);
}

void disableEndpointEvent(uint8_t bEP) {
    uint8_t endpoint = EP2IDX(bEP);

    // Disable an endpoint interrupt
    USBEpIntEn &= ~EP(endpoint);
}

bool realiseEndpoint(uint8_t bEP, uint32_t maxPacket, uint32_t flags)
{
    uint8_t endpoint = EP2IDX(bEP);

    // Realise an endpoint
    LPC_USB->USBDevIntClr = EP_RLZED;
    LPC_USB->USBReEp |= EP(endpoint);
    LPC_USB->USBEpInd = endpoint;
    LPC_USB->USBMaxPSize = maxPacket;

    setled(4, 1); while (!(LPC_USB->USBDevIntSt & EP_RLZED)); setled(4, 0);
    LPC_USB->USBDevIntClr = EP_RLZED;

    // Clear stall state
    unstallEndpoint(bEP);

    enableEndpointEvent(bEP);

    /*
     * if this is an OUT endpoint, enable interrupts so we can receive any
     *   data the host sends to us.
     *
     * if this is an IN endpoint, don't enable interrupts just yet, but have
     *   an event waiting so we can immediately interrupt later on when the
     *   user app calls endpointSetInterrupt(bEP, true)
     */

    if (IN_BEP(bEP))
    {
        can_transfer[endpoint] = 2;
    }
    else
    {
        can_transfer[endpoint] = 0;
        endpointSetInterrupt(bEP, true);
    }

    return true;
}

void stallEndpoint(uint8_t bEP)
{
    uint8_t endpoint = EP2IDX(bEP);

    // Stall an endpoint
    if (endpoint <= 1)
    {
        // Conditionally stall both control endpoints
        SIEsetEndpointStatus(IDX2EP(0), SIE_SES_CND_ST);
    }
    else
    {
        SIEsetEndpointStatus(bEP, SIE_SES_ST);

        // Update stall state
        endpointStallState |= EP(endpoint);
    }
}

void unstallEndpoint(uint8_t bEP)
{
    uint8_t endpoint = EP2IDX(bEP);

    // Unstall an endpoint. The endpoint will also be reinitialised
    SIEsetEndpointStatus(bEP, 0);

    // Update stall state
    endpointStallState &= ~EP(endpoint);
}

bool getEndpointStallState(uint8_t bEP)
{
    // Returns true if endpoint stalled
    return endpointStallState & EP(EP2IDX(bEP));
}

bool endpointSetInterrupt(uint8_t bEP, bool enabled)
{
    uint8_t endpoint = EP2IDX(bEP);

    bool r = USBEpIntEn | EP(endpoint);

    if (enabled)
    {
        __disable_irq();

        USBEpIntEn |= EP(endpoint);

        if (can_transfer[endpoint])
            endpointTriggerInterrupt(bEP);

        __enable_irq();
    }
    else
    {
        USBEpIntEn &= ~EP(endpoint);
    }

    return r;
}

bool endpointGetInterrupt(uint8_t bEP)
{
    uint8_t endpoint = EP2IDX(bEP);

    return USBEpIntEn | EP(endpoint);
}

void endpointTriggerInterrupt(uint8_t bEP)
{
    uint8_t endpoint = EP2IDX(bEP);

    LPC_USB->USBEpIntSet = EP(endpoint);
}

void     _ignore(uint32_t)
{
}

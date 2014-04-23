#include "USBhw.h"

#include "USBClient.h"

#include "USB_LPC17xx_SIE.h"

#include <LPC17xx.h>

// Power Control for Peripherals register
#define PCUSB      (1UL<<31)

// USB Clock Control register
#define DEV_CLK_EN (1UL<<1)
#define AHB_CLK_EN (1UL<<4)

// USB Clock Status register
#define DEV_CLK_ON (1UL<<1)
#define AHB_CLK_ON (1UL<<4)

/* USBRxPLen bits */
#define PKT_LNGTH                   (1<<0)
#define PKT_LNGTH_MASK              0x3FF
#define DV                          (1<<10)
#define PKT_RDY                     (1<<11)

USBhw* instance;

USBhw::USBhw(USBhw_callback_receiver* callback)
{
    instance = this;

    // Disable IRQ
    NVIC_DisableIRQ(USB_IRQn);

    // Enable power to USB device controller
    LPC_SC->PCONP |= PCUSB;

    // Enable USB clocks
    LPC_USB->USBClkCtrl |= DEV_CLK_EN | AHB_CLK_EN;
    while (LPC_USB->USBClkSt != (DEV_CLK_ON | AHB_CLK_ON));

    // Configure pins P0.29 and P0.30 to be USB D+ and USB D-
    LPC_PINCON->PINSEL1 &= 0xc3ffffff;
    LPC_PINCON->PINSEL1 |= 0x14000000;

    // Configure pin P2.9 to be Connect
    LPC_PINCON->PINSEL4 &= 0xfffcffff;
    LPC_PINCON->PINSEL4 |= 0x00040000;

    // Disconnect USB device
    SIEdisconnect();

    // work around OSX behaviour where if the device disconnects and quickly reconnects, it assumes it's the same device instead of checking
//     wait_ms(1000);

    // Set the maximum packet size for the control endpoints
    realiseEndpoint(IDX2EP(EP0IN), 64, 0);
    realiseEndpoint(IDX2EP(EP0OUT), 64, 0);

    // Attach IRQ
    //     instance = this;
    //     NVIC_SetVector(USB_IRQn, (uint32_t)&_usbisr);
    //     NVIC_EnableIRQ(USB_IRQn);

    USBEpIntEn = 0x3;

    // Enable interrupts for device events and EP0
    LPC_USB->USBDevIntEn = EP_SLOW | DEV_STAT | FRAME;
    //     enableEndpointEvent(EP0IN);
    //     enableEndpointEvent(EP0OUT);
}

void USBhw::connect()
{
    SIEconnect();

    usb_connect();
}

void USBhw::disconnect()
{
    usb_disconnect();

    SIEdisconnect();
}

uint8_t USBhw::index_to_endpoint(uint8_t index, USB_ENDPOINT_TYPES type)
{
    if (type == CONTROL)
        return 0;

    uint8_t r = type + (index * 3);

    // oddity in LPC17xx fixed endpoints
    if ((type == BULK) && (index == 5))
        r = 15;

    if (r > 15)
        return 255;

    return r;
}

uint8_t USBhw::read( uint8_t bEP, uint8_t* buffer)
{
    // Read from an OUT endpoint
    uint32_t size;
    uint32_t i;
    uint32_t data = 0;
    uint8_t  offset;
    uint8_t  endpoint = EP2IDX(bEP);

    volatile uint32_t dummyRead;

    uint8_t irq = NVIC_GetActive(USB_IRQn);
    NVIC_DisableIRQ(USB_IRQn);

    LPC_USB->USBCtrl = LOG_ENDPOINT(endpoint) | RD_EN;

    while (!(LPC_USB->USBRxPLen & PKT_RDY));

    size = LPC_USB->USBRxPLen & PKT_LNGTH_MASK;

    if ((IS_ISOCHRONOUS(bEP) == 0) && (size > 64))
        size = 64;

    offset = 0;

    if (size > 0)
    {
        for (i = 0; i < size; i++)
        {
            if (offset == 0)
                // Fetch up to four bytes of data as a word
                data = LPC_USB->USBRxData;

            // extract a byte
            *buffer = (data>>offset) & 0xff;
            buffer++;

            // move on to the next byte
            offset = (offset + 8) & 24;
        }
    }
    else
    {
        dummyRead = LPC_USB->USBRxData;
    }

    while ((LPC_USB->USBDevIntSt & RxENDPKT) == 0)
        dummyRead = LPC_USB->USBRxData;

    _ignore(dummyRead);

    if (can_transfer[endpoint] != 0)
        can_transfer[endpoint]--;

    LPC_USB->USBCtrl = 0;

    if (IS_ISOCHRONOUS(bEP) == 0)
    {
        SIEselectEndpoint(bEP);
        SIEclearBuffer();
    }

    if (irq)
        NVIC_EnableIRQ(USB_IRQn);

    return size;
}

uint8_t USBhw::write(uint8_t bEP, uint8_t* buffer, uint8_t length)
{
    // Write to an IN endpoint
    //     uint32_t temp, data;
    //     uint8_t offset;
    uint8_t endpoint = EP2IDX(bEP);

    LPC_USB->USBCtrl = LOG_ENDPOINT(endpoint) | WR_EN;

    LPC_USB->USBTxPLen = length;

    while (LPC_USB->USBCtrl & WR_EN)
    {
        LPC_USB->USBTxData = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
        buffer += 4;
    }

    // Clear WR_EN to cover zero length packet case
    LPC_USB->USBCtrl = 0;

    SIEselectEndpoint(bEP);
    SIEvalidateBuffer();

    return length;
}

extern "C" {
    __attribute__ ((interrupt)) void USB_IRQHandler() {
        USBhw::instance->usbisr();
    }
}
void USBhw::usbisr(void)
{
    if (LPC_USB->USBDevIntSt & FRAME)
    {
        // Clear interrupt status flag
        LPC_USB->USBDevIntClr = FRAME;

        usb_sof(SIEgetFrameNumber());
    }

    if (LPC_USB->USBDevIntSt & DEV_STAT)
    {
        // Device Status interrupt
        // Must clear the interrupt status flag before reading the device status from the SIE
        LPC_USB->USBDevIntClr = DEV_STAT;

        // Read device status from SIE
        uint8_t devStat = SIEgetDeviceStatus();
        //printf("devStat: %d\r\n", devStat);

        if (devStat & SIE_DS_SUS_CH)
        {
            // Suspend status changed
            if (devStat & SIE_DS_SUS)
                usb_suspend();
            else
                usb_wake();
        }

        if (devStat & SIE_DS_RST)
        {
            // Bus reset
            usb_reset();

            realiseEndpoint(IDX2EP(EP0IN), 64, 0);
            realiseEndpoint(IDX2EP(EP0OUT), 64, 0);

            SIEsetMode(0);
        }

        if (devStat & SIE_DS_CON_CH)
        {
            if (devStat & SIE_DS_CON)
                usb_connect();
            else
                usb_disconnect();
        }
    }

    if (LPC_USB->USBDevIntSt & EP_SLOW)
    {
        // (Slow) Endpoint Interrupt

        // clear interrupt
        LPC_USB->USBDevIntClr = EP_SLOW;

        // Process each endpoint interrupt
        if (LPC_USB->USBEpIntSt & EP(EP0OUT))
        {
            uint8_t bEPStat = selectEndpointClearInterrupt(IDX2EP(EP0OUT));

            if (bEPStat & SIE_SE_STP) // this is a setup packet
                usb_setup();
            else if (bEPStat & EPSTAT_FE) // OUT endpoint, FE = 1 - data in buffer
                usb_endpoint_rx(EP0OUT);
        }
        if (LPC_USB->USBEpIntSt & EP(EP0IN))
        {
            uint8_t bEPStat = selectEndpointClearInterrupt(IDX2EP(EP0IN));

            if ((bEPStat & EPSTAT_FE) == 0) // IN endpoint, FE = 0 - empty space in buffer
                usb_endpoint_tx_complete(EP0IN);
        }

        if (USBEpIntEn & ~(3UL))
        {
            int i;
            uint32_t bitmask;

            for (i = 2, bitmask = 4; i < 32; i++, bitmask <<= 1)
            {
                uint8_t bEPStat = 255;
                uint8_t ep = IDX2EP(i);
                if (LPC_USB->USBEpIntSt & bitmask)
                {
                    bEPStat = selectEndpointClearInterrupt(ep);
                    if (can_transfer[i] < 2)
                        can_transfer[i]++;
                }

                if ((USBEpIntEn & bitmask) && (can_transfer[i]))
                {
                    if (bEPStat == 255)
                        bEPStat = SIEselectEndpoint(ep);

//                     uint8_t bStat = ((bEPStat & EPSTAT_FE ) ? EP_STATUS_DATA    : 0) |
//                     ((bEPStat & EPSTAT_ST ) ? EP_STATUS_STALLED : 0) |
//                     ((bEPStat & EPSTAT_STP) ? EP_STATUS_SETUP   : 0) |
//                     ((bEPStat & EPSTAT_EPN) ? EP_STATUS_NACKED  : 0) |
//                     ((bEPStat & EPSTAT_PO ) ? EP_STATUS_ERROR   : 0);

                    bool r = true;

                    if (IN_EP(i))
                    {
                        if ((bEPStat & EPSTAT_FE) == 0) // IN endpoint, FE = 0 - empty space in buffer
                        {
                            if (bEPStat & EPSTAT_EPN)
                                r = usb_endpoint_tx_nak(ep);
                            else
                                r = usb_endpoint_tx_complete(ep);
                        }
                    }
                    else
                    {
                        if (bEPStat & EPSTAT_FE) // OUT endpoint, FE = 1 - data in buffer
                            r = usb_endpoint_rx(ep);
                        else
                            r = usb_endpoint_rx_nak(ep);
                    }

                    if (!r)
                    {
                        USBEpIntEn &= ~bitmask;
                    }
                }
            }
        }
    }
}

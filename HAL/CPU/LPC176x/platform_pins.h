#ifndef _PINS_PLATFORM_H
#define _PINS_PLATFORM_H

#include "pins.h"

/*
 * LPC176x pins and capabilities
 * 
 *  PIN    ALT01     ALT02     ALT03
 * -------------------------------------------------------------
 * P0_0    RD1       TXD3      SDA1
 * P0_1    TD1       RXD3      SCL1
 * P0_2    TXD0      AD0.7     -
 * P0_3    RXD0      AD0.6     -
 * P0_4    I2SRX_CLK RD2       CAP2.0
 * P0_5    I2SRX_WS  TD2       CAP2.1
 * P0_6    I2SRX_SDA SSEL1     MAT2.0
 * P0_7    I2STX_CLK SCK1      MAT2.1
 * P0_8    I2STX_WS  MISO1     MAT2.2
 * P0_9    I2STX_SDA MOSI1     MAT2.3
 * P0_10   TXD2      SDA2      MAT3.0
 * P0_11   RXD2      SCL2      MAT3.1
 * 
 * P0_15   TXD1      SCK0      SCK
 * P0_16   RXD1      SSEL0     SSEL
 * P0_17   CTS1      MISO0     MISO
 * P0_18   DCD1      MOSI0     MOSI
 * P0_19   DSR1      -         SDA1
 * P0_20   DTR1      -         SCL1
 * P0_21   RI1       -         RD1
 * P0_22   RTS1      -         TD1
 * P0_23   AD0.0     I2SRX_CLK CAP3.0
 * P0_24   AD0.1     I2SRX_WS  CAP3.1
 * P0_25   AD0.2     I2SRX_SDA TXD3
 * P0_26   AD0.3     AOUT      RXD3
 * P0_27   SDA0      USB_SDA   -
 * P0_28   SCL0      USB_SCL   -
 * P0_29   USB_D+    -         -
 * P0_30   USB_D-    -         -
 * 
 * P1_0    ENET_TXD0
 * P1_1    ENET_TXD1
 * 
 * P1_4    ENET_TX_EN
 * 
 * P1_8    ENET_CRS
 * P1_9    ENET_RXD0
 * P1_10   ENET_RXD1
 * 
 * P1_14   ENET_RX_ER
 * P1_15   ENET_REF_CLK
 * P1_16   ENET_MDC
 * P1_17   ENET_MDIO
 * P1_18   USB_UP_LED PWM1.1   CAP1.0
 * P1_19   MCOA0     USB_PPWR  CAP1.1
 * P1_20   MCI0      PWM1.2    SCK0
 * P1_21   MCABORT   PWM1.3    SSEL0
 * P1_22   MCOB0     USB_PWRD  MAT1.0
 * P1_23   MCI1      PWM1.4    MISO0
 * P1_24   MCI2      PWM1.5    MOSI0
 * P1_25   MCOA1     -         MAT1.1
 * P1_26   MCOB1     PWM1.6    CAP0.0
 * P1_27   CLKOUT    USB_OVRCR CAP0.1
 * P1_28   MCOA2     PCAP1.0   MAT0.0
 * P1_29   MCOB2     PCAP1.1   MAT0.1
 * P1_30   -         Vbus      AD0.4
 * P1_31   -         SCK1      AD0.5
 * 
 * P2_0    PWM1.1    TXD1      -
 * P2_1    PWM1.2    RXD1      -
 * P2_2    PWM1.3    CTS1      -
 * P2_3    PWM1.4    DCD1      -
 * P2_4    PWM1.5    DSR1      -
 * P2_5    PWM1.6    DTR1      -
 * P2_6    PCAP1.0   RI1       -
 * P2_7    RD2       RTS1      -
 * P2_8    TD2       TXD2      ENET_MDC
 * P2_9    USB_CONN  RXD2      ENET_MDIO
 * P2_10   EINT0     NMI       -
 * P2_11   EINT1     -         I2STX_CLK
 * P2_12   EINT2     -         I2STX_WS
 * P2_13   EINT3     -         I2STX_SDA
 * 
 * P3_25   -         MAT0.0    PWM1.2
 * P3_26   STCLK     MAT0.1    PWM1.3
 * 
 * P4_28   RX_MCLK   MAT2.0    TXD3
 * P4_29   TX_MCLK   MAT2.1    RXD3
 */

// mbed DIP Pin Names
#define p5  P0_9
#define p6  P0_8
#define p7  P0_7
#define p8  P0_6
#define p9  P0_0
#define p10 P0_1
#define p11 P0_18
#define p12 P0_17
#define p13 P0_15
#define p14 P0_16
#define p15 P0_23
#define p16 P0_24
#define p17 P0_25
#define p18 P0_26
#define p19 P1_30
#define p20 P1_31
#define p21 P2_5
#define p22 P2_4
#define p23 P2_3
#define p24 P2_2
#define p25 P2_1
#define p26 P2_0
#define p27 P0_11
#define p28 P0_10
#define p29 P0_5
#define p30 P0_4

#define UART0_TX P0_2
#define UART0_RX P0_3

#define SSP0_SCK  P0_15
#define SSP0_SS   P0_16
#define SSP0_MISO P0_17
#define SSP0_MOSI P0_18

#define SSP0_ALT0_SCK  P1_20
#define SSP0_ALT0_SS   P1_21
#define SSP0_ALT0_MISO P1_23
#define SSP0_ALT0_MOSI P1_24

#define SSP1_SS   P0_6
#define SSP1_SCK  P0_7
#define SSP1_MISO P0_8
#define SSP1_MOSI P0_9

#endif /* _PINS_PLATFORM_H */

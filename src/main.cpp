
/*****************************************************************************
 *                                                                            *
 * DFU/SD/SDHC Bootloader for LPC17xx                                         *
 *                                                                            *
 * by Triffid Hunter                                                          *
 *                                                                            *
 *                                                                            *
 * This firmware is Copyright (C) 2009-2010 Michael Moon aka Triffid_Hunter   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software                *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include "pins_platform.h"
#include "pindef.h"

#include "debug.h"

#include "min-printf.h"

#include "mri.h"

#include "Serial.h"
#include "gpio.h"
#include "MemoryPool.h"

#include "SPI.h"

#include "platform_utils.h"

// Serial uart(UART0_TX, UART0_RX, APPBAUD);
Serial* uart = NULL;

extern "C" {
	int _write(int fd, const char *buf, int buflen)
	{
		int r;
		int sl = buflen;
		if (fd < 3)
		{
			if (uart == NULL)
				return buflen;
			while (buflen)
			{
				r = uart->write(buf, buflen);
				buflen -= r;
				buf += r;
				if (buflen)
					asm volatile ("wfi");
			}
		}
		return sl;
	}
	
	void setleds(GPIO* leds, int l)
	{
		for (int i = 0, j = 1; i < N_LEDS; i++, j<<=1)
		{
			leds[i].write(l & j);
		}
	}
}

int sd_cmd(SPI* spi, int cmd, uint32_t arg)
{
	union {
		struct __attribute__ ((packed)) {
			uint8_t cmd;
			uint32_t arg;
			uint8_t checksum;
		};
		uint8_t packet[6];
	} spi_cmd0;
	
	uint8_t rxbuf[8];
	
	printf("CMD%d: ", cmd);
	
	spi->begin_transaction();
	
	spi_cmd0.cmd = 0x40 | cmd;
	spi_cmd0.arg = htonl(arg);
	if (cmd == 0)
		spi_cmd0.checksum = 0x95;
	else if (cmd == 8)
		spi_cmd0.checksum = 0x87;
	else
		spi_cmd0.checksum = 0xAA;
	
	printf("Send: ");
	
	for (uint32_t j = 0; j < sizeof(spi_cmd0); j++)
	{
		printf("0x%X ", spi_cmd0.packet[j]);
	}
	
	spi->send_block(spi_cmd0.packet, sizeof(spi_cmd0));
	
	printf("Recv: ");
	
// 	spi->recv_block(rxbuf, 8, 0xFF);
	int i;
	for (i = 0; i < 8; i++)
	{
		rxbuf[i] = spi->transfer(0xFF);
		printf("0x%X ", rxbuf[i]);
		if (rxbuf[i] != 0xFF)
			break;
	}
	
	spi->end_transaction();
	
	printf("\nCMD%d complete!\n", cmd);
	
	return rxbuf[i];
}

int sd_cmd_data(SPI* spi, int cmd, uint32_t arg, uint8_t* buf, int buflen)
{
	union {
		struct __attribute__ ((packed)) {
			uint8_t cmd;
			uint32_t arg;
			uint8_t checksum;
		};
		uint8_t packet[6];
	} spi_cmd0;
	
	uint8_t rxbuf[8];
	
	printf("CMD%d: ", cmd);
	
	spi->begin_transaction();
	
	spi_cmd0.cmd = 0x40 | cmd;
	spi_cmd0.arg = htonl(arg);
	if (cmd == 0)
		spi_cmd0.checksum = 0x95;
	else if (cmd == 8)
		spi_cmd0.checksum = 0x87;
	else
		spi_cmd0.checksum = 0xAA;
	
	printf("Send: ");
	
	for (uint32_t j = 0; j < sizeof(spi_cmd0); j++)
	{
		printf("0x%X ", spi_cmd0.packet[j]);
	}
	
	spi->send_block(spi_cmd0.packet, sizeof(spi_cmd0));
	
	printf("Recv: ");
	
	// 	spi->recv_block(rxbuf, 8, 0xFF);
	int i;
	for (i = 0; i < 8; i++)
	{
		rxbuf[i] = spi->transfer(0xFF);
		printf("0x%X ", rxbuf[i]);
		if (rxbuf[i] != 0xFF)
			break;
	}
	
	// read data
	for (i = 0; i != 0xFE; i = spi->transfer(0xFF));
	spi->recv_block(buf, buflen);
	
	spi->end_transaction();
	
	printf("\nCMD%d complete!\n", cmd);
	
	return rxbuf[i];
}

int sd_cmd8(SPI* spi)
{
	uint8_t buf[4];
	int r = sd_cmd(spi, 8, 0x1AA);
	spi->recv_block(buf, 4, 0xFF);
	for (int i = 0; i < 4; i++)
		printf("0x%X ", buf[i]);
	printf("\n");
	return r;
}

int sd_cmd58(SPI* spi)
{
	union {
		uint8_t rx[8];
		struct {
			uint8_t response;
			uint32_t ocr;
			uint8_t fill[3];
		};
	} rxbuf;
	
	rxbuf.response = 0x40 | 58;
	rxbuf.ocr = htonl(0);
	rxbuf.fill[0] = 0xAA;
	
	printf("CMD58: ");
	
	spi->begin_transaction();
	
	printf("Send: ");
	
	for (int i = 0; i < 6; i++)
		printf("0x%X ", rxbuf.rx[i]);
	
	spi->send_block(rxbuf.rx, 6);
	
	printf("Recv: ");
	
	rxbuf.rx[0] = 0xFF;
	while (rxbuf.rx[0] == 0xFF)
	{
		rxbuf.rx[0] = spi->transfer(0xFF);
		printf("0x%X ", rxbuf.rx[0]);
	}
	spi->recv_block(rxbuf.rx + 1, 4, 0xFF);
	
	for (int i = 0; i < 4; i++)
		printf("0x%X ", rxbuf.rx[i + 1]);
	
	uint32_t ocr = ntohl(rxbuf.ocr);
	
	printf("CMD58 Resp: %d OCR: 0x%lX\n", rxbuf.response, ocr);
	
	if (ocr & 0x80000000)
		printf("Card Startup complete\n");
	else
		printf("Card Startup incomplete\n");
	
	if (ocr & 0x40000000)
		printf("Card is High capacity\n");
	else
		printf("Card is Standard capacity\n");
	
	printf("Card supported voltages:\n");
	if (ocr & (1<<23))
		printf("\t3.5 - 3.6v\n");
	if (ocr & (1<<22))
		printf("\t3.4 - 3.5v\n");
	if (ocr & (1<<21))
		printf("\t3.3 - 3.4v\n");
	if (ocr & (1<<20))
		printf("\t3.2 - 3.3v !\n");
	else
		printf("\tNOT SUPPORT 3.2 - 3.3v !!!\n");
	if (ocr & (1<<19))
		printf("\t3.1 - 3.2v\n");
	if (ocr & (1<<18))
		printf("\t3.0 - 3.1v\n");
	if (ocr & (1<<17))
		printf("\t2.9 - 3.0v\n");
	if (ocr & (1<<16))
		printf("\t2.8 - 2.9v\n");
	if (ocr & (1<<15))
		printf("\t2.7 - 2.8v\n");
	
	spi->end_transaction();
	
	return rxbuf.response;
}

int sd_recv_data(SPI* spi, uint8_t* buf, int buflen)
{
	buf[0] = 0;
	
	printf("Recv block, expect %db... ", buflen);
	
	// wait for START TRAN token
	while (buf[0] != 0xFE)
	{
		buf[0] = spi->transfer(0xFF);
		printf("0x%X ", buf[0]);
	}
	
	printf("Start Tran\n");
	
	// receive block
// 	spi->recv_block(buf, buflen, 0xFF);
	
	__debugbreak();
	
	// receive buffer
	DMA_mem rx(buf, buflen);
	
	// transmit "buffer"
	uint8_t x = 0xFF;
	DMA_mem tx(&x, buflen);
	tx.auto_increment = DMA_NO_INCREMENT;
	
	DMA dma_tx(&tx, spi);
	DMA dma_rx(spi, &rx);
	
	dma_rx.begin(512);
	dma_tx.begin(512);
	
	__debugbreak();
	
	while (dma_rx.running());
	
	for (int i = 0; i < buflen; i++)
		printf("0x%X ", buf[i]);
	
	// consume checksum
	printf("\n(checksum) 0x%X ", spi->transfer(0xFF));
	printf(             "0x%X\n", spi->transfer(0xFF));
	
}

int main()
{
	__mriInit("MRI_UART_0 MRI_UART_SHARE MRI_UART_BAUD=1000000");
	
	GPIO leds[5] = {
		GPIO(LED1),
		GPIO(LED2),
		GPIO(LED3),
		GPIO(LED4),
		GPIO(LED5)
	};
	
	int i = 0;
	
	for (i = 0; i < N_LEDS; i++)
	{
		leds[i].output();
	}
	
	setleds(leds, 0);
	
	uart = new Serial(UART0_TX, UART0_RX, APPBAUD);

	setleds(leds, 1);
	
	uart->write("Start\n", 6);
	
// 	__debugbreak();
	
	printf("Start 2\n");
	
	setleds(leds, 2);
	
	// 	__debugbreak();
	printf("testing a large string which should hopefully demonstrate whether or not the serial queueing mechanisms are sensible.\nThe length of this string has been extended to overrun the serial transmit buffer.\n");

	if (0)
	{
		volatile uint32_t r;
		i = 10;

		for (;;)
		{
			printf("- %d\n", i);
			setleds(leds, i++);
			i &= 31;
			for (r = 0; r < (1<<20); r++);
		}
	}
	
	SPI* spi = new SPI(SSP1_MOSI, SSP1_MISO, SSP1_SCK, SSP1_SS);
	
	printf("Starting SD Card test\n");

	sd_cmd(spi, 0, 0);
	sd_cmd8(spi);
	sd_cmd58(spi);
	do {
		sd_cmd(spi, 55, 0);
		i = sd_cmd(spi, 41, (1<<30));
	} while (i == 1);
	sd_cmd58(spi);
	
	uint8_t rxbuf[512];
	sd_cmd(spi, 17, 0);
	sd_recv_data(spi, rxbuf, 512);

	for(;;);
}


// 	void NMI_Handler() {
// 		DEBUG_PRINTF("NMI\n");
// 		for (;;);
// 	}
// 	void HardFault_Handler() {
// 		DEBUG_PRINTF("HardFault\n");
// 		for (;;);
// 	}
// 	void MemManage_Handler() {
// 		DEBUG_PRINTF("MemManage\n");
// 		for (;;);
// 	}
// 	void BusFault_Handler() {
// 		DEBUG_PRINTF("BusFault\n");
// 		for (;;);
// 	}
// 	void UsageFault_Handler() {
// 		DEBUG_PRINTF("UsageFault\n");
// 		for (;;);
// 	}
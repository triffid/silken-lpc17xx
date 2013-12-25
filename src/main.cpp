
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

#include "platform_pins.h"
#include "pindef.h"

#include "debug.h"

#include "mri.h"

#include "Serial.h"
#include "gpio.h"
#include "MemoryPool.h"
#include "clock.h"

#include "SPI.h"
#include "SD.h"
#include "fat.h"

#include "platform_utils.h"
#include "platform_memory.h"

// uint8_t athing[32] __attribute__ ((section(".AHB0")));

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

/*
// int sd_cmdx(SPI* spi, int cmd, uint32_t arg)
// {
// 	union {
// 		struct __attribute__ ((packed)) {
// 			uint32_t dummy;
// 			uint8_t cmd;
// 			uint32_t arg;
// 			uint8_t checksum;
// 			uint8_t dummy1;
// 		};
// 		uint8_t packet[11];
// 	} spi_cmd0;
//
// 	uint8_t rxbuf[8];
//
// 	printf("CMD%d: ", cmd);
//
// 	spi->begin_transaction();
//
// 	// dummy transfers, not sure why these are necessary but they are
// // 	spi->send_block(rxbuf, 4);
//
// 	spi_cmd0.dummy = 0xFFFFFFFF;
// 	spi_cmd0.dummy1 = 0xFF;
// 	spi_cmd0.cmd = 0x40 | cmd;
// 	spi_cmd0.arg = htonl(arg);
// 	if (cmd == 8)
// 		spi_cmd0.checksum = 0x87;
// 	else
// 		spi_cmd0.checksum = 0x95;
//
// 	printf("Send: ");
//
// 	for (uint32_t j = 0; j < sizeof(spi_cmd0); j++)
// 	{
// 		printf("0x%X ", spi_cmd0.packet[j]);
// 	}
//
// 	spi->send_block(spi_cmd0.packet, sizeof(spi_cmd0));
//
// 	printf("Recv: ");
//
// // 	spi->recv_block(rxbuf, 8, 0xFF);
// 	int i;
// 	for (i = 0; i < 8; i++)
// 	{
// 		rxbuf[i] = spi->transfer(0xFF);
// 		printf("0x%X ", rxbuf[i]);
// 		if (rxbuf[i] != 0xFF)
// 			break;
// 	}
//
// 	if (i >= 8)
// 	{
// 		printf("Error! CMD failed!\n");
// 		for (volatile int z = 1<<18; z; z--);
// 		__debugbreak();
// 	}
//
// 	printf("\nCMD%d complete!\n", cmd);
//
// 	return rxbuf[i];
// }
//
// int sd_cmd(SPI* spi, int cmd, uint32_t arg)
// {
// 	int r = sd_cmdx(spi, cmd, arg);
// 	spi->end_transaction();
//
// 	return r;
// }
//
// int sd_cmd_data(SPI* spi, int cmd, uint32_t arg, uint8_t* buf, int buflen)
// {
// 	union {
// 		struct __attribute__ ((packed)) {
// 			uint8_t cmd;
// 			uint32_t arg;
// 			uint8_t checksum;
// 		};
// 		uint8_t packet[6];
// 	} spi_cmd0;
//
// 	uint8_t rxbuf[8];
//
// 	printf("CMD%d: ", cmd);
//
// 	spi_cmd0.cmd = 0x40 | cmd;
// 	spi_cmd0.arg = htonl(arg);
// 	if (cmd == 0)
// 		spi_cmd0.checksum = 0x95;
// 	else if (cmd == 8)
// 		spi_cmd0.checksum = 0x87;
// 	else
// 		spi_cmd0.checksum = 0xAA;
//
// 	printf("Send: ");
//
// 	for (uint32_t j = 0; j < sizeof(spi_cmd0); j++)
// 	{
// 		printf("0x%X ", spi_cmd0.packet[j]);
// 	}
//
// 	spi->begin_transaction();
//
// 		spi->send_block(spi_cmd0.packet, sizeof(spi_cmd0));
//
// 		printf("Recv: ");
//
// 		int i;
// 		for (i = 0; i < 8; i++)
// 		{
// 			rxbuf[i] = spi->transfer(0xFF);
// 			printf("0x%X ", rxbuf[i]);
// 			if (rxbuf[i] != 0xFF)
// 				break;
// 			if (i == 7)
// 				return -1;
// 		}
//
// 		printf("\n");
//
// 		// read data
// 		for (i = 0; i != 0xFE; i = spi->transfer(0xFF));
//
// 		spi->recv_block(buf, buflen, 0xFF);
//
// 	spi->end_transaction();
//
// 	printf("\nCMD%d complete!\n", cmd);
//
// 	return rxbuf[0];
// }
//
// int sd_cmd8(SPI* spi)
// {
// 	uint8_t buf[4];
//
// 	int r = sd_cmdx(spi, 8, 0x1AA);
//
// 		spi->recv_block(buf, 4, 0xFF);
//
// 	spi->end_transaction();
//
// 	for (int i = 0; i < 4; i++)
// 		printf("0x%X ", buf[i]);
//
// 	printf("\n");
//
// 	return r;
// }
//
// int sd_cmd58(SPI* spi)
// {
// 	union {
// 		uint8_t rx[8];
// 		struct {
// 			uint8_t response;
// 			uint32_t ocr;
// 			uint8_t fill[3];
// 		};
// 	} rxbuf;
//
// 	rxbuf.response = 0x40 | 58;
// 	rxbuf.ocr = htonl(0);
// 	rxbuf.fill[0] = 0xAA;
//
// 	printf("CMD58: ");
//
// 	printf("Send: ");
//
// 	for (int i = 0; i < 6; i++)
// 		printf("0x%X ", rxbuf.rx[i]);
//
// 	spi->begin_transaction();
//
// 		spi->send_block(rxbuf.rx, 6);
//
// 		printf("Recv: ");
//
// 		rxbuf.rx[0] = 0xFF;
// 		while (rxbuf.rx[0] == 0xFF)
// 		{
// 			rxbuf.rx[0] = spi->transfer(0xFF);
// 			printf("0x%X ", rxbuf.rx[0]);
// 		}
// 		spi->recv_block(rxbuf.rx + 1, 4, 0xFF);
//
// 	spi->end_transaction();
//
// 	for (int i = 0; i < 4; i++)
// 		printf("0x%X ", rxbuf.rx[i + 1]);
//
// 	uint32_t ocr = ntohl(rxbuf.ocr);
//
// 	printf("CMD58 Resp: %d OCR: 0x%lX\n", rxbuf.response, ocr);
//
// 	if (ocr & 0x80000000)
// 		printf("Card Startup complete\n");
// 	else
// 		printf("Card Startup incomplete\n");
//
// 	if (ocr & 0x40000000)
// 		printf("Card is High capacity\n");
// 	else
// 		printf("Card is Standard capacity\n");
//
// 	printf("Card supported voltages:\n");
// 	if (ocr & (1<<23))
// 		printf("\t3.5 - 3.6v\n");
// 	if (ocr & (1<<22))
// 		printf("\t3.4 - 3.5v\n");
// 	if (ocr & (1<<21))
// 		printf("\t3.3 - 3.4v\n");
// 	if (ocr & (1<<20))
// 		printf("\t3.2 - 3.3v !\n");
// 	else
// 		printf("\tNOT SUPPORT 3.2 - 3.3v !!!\n");
// 	if (ocr & (1<<19))
// 		printf("\t3.1 - 3.2v\n");
// 	if (ocr & (1<<18))
// 		printf("\t3.0 - 3.1v\n");
// 	if (ocr & (1<<17))
// 		printf("\t2.9 - 3.0v\n");
// 	if (ocr & (1<<16))
// 		printf("\t2.8 - 2.9v\n");
// 	if (ocr & (1<<15))
// 		printf("\t2.7 - 2.8v\n");
//
// 	return rxbuf.response;
// }
//
// #define USE_DMA
//
// int sd_recv_data(SPI* spi, uint8_t* buf, int buflen)
// {
// 	buf[0] = 0;
// 	int r;
//
// 	printf("Recv block, expect %db... ", buflen);
//
// 	int i = 1<<9;
// 	// wait for START TRAN token
// 	while (buf[0] != 0xFE)
// 	{
// 		buf[0] = spi->transfer(0xFF);
// 		printf("0x%X ", buf[0]);
// 		if (i-- <= 0)
// 		{
// 			printf("Error! timeout waiting for START TRAN token\n");
// 			return -1;
// 		}
// 	}
//
// 	r = buf[0];
//
// 	printf("Start Tran\n");
//
// 	// receive block
// #ifndef USE_DMA
// 	spi->recv_block(buf, buflen, 0xFF);
// #endif
//
// // 	__debugbreak();
//
// #ifdef USE_DMA
// 	// receive buffer
// 	DMA_mem rx(buf, buflen);
//
// 	// transmit "buffer"
// 	uint32_t x = 0xFFFFFFFF;
// 	DMA_mem tx(&x, 4);
//
// 	tx.auto_increment = DMA_NO_INCREMENT;
//
// 	DMA dma_tx(&tx, spi);
// 	DMA dma_rx(spi, &rx);
//
// 	for (int i = 0; i < 512; i++)
// 		buf[i] = 0xFF;
//
// 	dma_rx.setup(512);
// 	dma_tx.setup(512);
//
// // 	LPC_GPDMA->DMACEnbldChns |= (1<<7) | (1<<6);
// 	dma_rx.begin();
// 	dma_tx.begin();
//
// 	while (dma_rx.running())
// 		__WFI();
//
// 	while (0)
// 	{
// // 		dma_tx.debug();
// // 		dma_rx.debug();
// 		int t = dma_tx.running();
// 		int r = dma_rx.running();
// 		printf("tx: %d rx:%d\n\n", t, r);
// 		if (r == 0)
// 			break;
// 	}
//
// // 	for (volatile int z = 0x7FFFFFFF; z > 0 && dma_rx.running(); z--)
// // 		if ((z & 0xFFFF) == 0)
// // 			dma_rx.debug();
//
// #endif
// 	printf("(buf %p)\n", buf);
// 	for (int i = 0; i < buflen; i++)
// 		printf("0x%X%c", buf[i], (((i & 31) == 31)?'\n':' '));
//
// #ifdef USE_DMA
// // 	dma_tx.debug();
// // 	dma_rx.debug();
//
// 	spi->dma_complete(&dma_tx, DMA_SENDER);
// 	spi->dma_complete(&dma_rx, DMA_RECEIVER);
// #endif
//
// 	// consume checksum
// 	printf("\n(checksum) 0x%X ", spi->transfer(0xFF));
// 	printf(             "0x%X\n", spi->transfer(0xFF));
//
// 	spi->transfer(0xFF);
//
// // 	spi->end_transaction();
//
// 	return r;
// }
//*/
class sar : public SD_async_receiver
{
public:
    sar()
    {
        last_sector = 0;
    }
	void sd_read_complete(SD* sd, uint32_t sector, void* buf, int err)
	{
// 		uint8_t* b = (uint8_t*) buf;
		printf("read complete! sector %lu, buf %p, err: %d\n", sector, buf, err);
		if (err == 0)
		{
// 			for (int i = 0; i < 512; i++)
// 				printf("0x%X%c", b[i], ((i & 31) == 31)?'\n':' ');
            sd->clean_buffer(buf);
            last_sector = sector;
        }

// 		if (sector == 0)
// 			sd->begin_read(133, buf, this);
// 		else if (sector >= 133 && sector <= 150)
// 			sd->begin_read(sector + 1, buf, this);
	};

	void sd_write_complete(SD* sd, uint32_t sector, void* buf, int err)
	{
	};

    uint32_t last_sector;
} sar_dumper;

GPIO* isp_btn;

int main()
{
	int i = 0;
	
	__mriInit("MRI_UART_0 MRI_UART_SHARE MRI_UART_BAUD=1000000");
	
	// 3 bits for group, 2 bits subgroup
	NVIC_SetPriorityGrouping(4);

	// set all interrupts to low priority
	for (i = 0; i < 34; i++)
		NVIC_SetPriority((IRQn_Type) i, 31);

	// UART gets highest priority
	NVIC_SetPriority(UART0_IRQn, 0);

	GPIO leds[5] = {
		GPIO(LED1),
		GPIO(LED2),
		GPIO(LED3),
		GPIO(LED4),
		GPIO(LED5)
	};

    isp_btn = new GPIO(P2_10);
	
	for (i = 0; i < N_LEDS; i++)
	{
		leds[i].output();
	}
	
	setleds(leds, 0);
	
	uart = new Serial(UART0_TX, UART0_RX, APPBAUD);

	setleds(leds, 1);

//     athing[0] = 'S';
//     athing[1] = 't';
//     athing[2] = 'a';
//     athing[3] = 'r';
//     athing[4] = 't';
//     athing[5] = '\n';
//     athing[6] = 0;
	
// 	uart->write(athing, 6);
    uart->write("Start\n", 6);
	
	printf("Start 2\n");

	setleds(leds, 2);

	printf("testing a large string which should hopefully demonstrate whether or not the serial queueing mechanisms are sensible.\nThe length of this string has been extended to overrun the serial transmit buffer.\n");
	
	SPI* spi = new SPI(SSP1_MOSI, SSP1_MISO, SSP1_SCK, SSP1_SS);
	
	printf("Starting SD Card test\n");

	SD* sd = new SD(spi);

	int r;
	if ((r = sd->init()) < 1)
	{
		printf("SD init failed: %d!\n", r);
		for(;;)
			__WFI();
	}

// 	uint8_t* rbuf = (uint8_t*) AHB0.alloc(512);

// 	sd->begin_read(0, rbuf, &sar_dumper);
// 	for (int z = 0; z < 32; z++)
// 		sd->begin_read(133 + z, rbuf, &sar_dumper);

	Fat* fat = new Fat();

	_fat_mount_ioresult fmount;

	fat->f_mount(&fmount, sd);

	while (fmount.fini == 0)
		sd->on_idle();

	printf("Mounted!\n");

    Clock clock;

    uint32_t clockflag = clock.request_flag();

    uint8_t* buf = (uint8_t*) AHB0.alloc(512);

    printf("Begin read Sectors 0-255\n");
    sd->begin_read(0, 256, buf, &sar_dumper);

    for (;;)
    {
        if (isp_btn->get() == 0)
            __debugbreak();
		sd->on_idle();
        if (clock.flag_1s_test(clockflag))
        {
            if (sar_dumper.last_sector >= 255)
            {
                printf("Time is %llu\n", clock.time_s());
                printf("Begin read Sectors 0-255\n");
                sd->begin_read(0, 256, buf, &sar_dumper);
            }
        }
    }

// 	sd_cmd(spi, 0, 0);
// 	sd_cmd8(spi);
// 	sd_cmd58(spi);
// 	do {
// 		sd_cmd(spi, 55, 0);
// 		i = sd_cmd(spi, 41, (1<<30));
// 	} while (i == 1);
// 	sd_cmd58(spi);
//
// 	spi->set_frequency(10000000);
//
// 	uint8_t* rxbuf = (uint8_t*) AHB0.alloc(512);
//
// 	sd_cmdx(spi, 17, 0 * 512); sd_recv_data(spi, rxbuf, 512);
// 	sd_cmdx(spi, 18, 133 * 512);
// // 	sd_recv_data(spi, rxbuf, 512);
// // 	sd_cmdx(spi, 17, 134 * 512); sd_recv_data(spi, rxbuf, 512);
//
// 	for (i = 0; i < 16; i++)
// 		sd_recv_data(spi, rxbuf, 512);
//
// 	sd_cmd(spi, 12, 0);
//
// 	printf("DMA IntStat: %lu\n", *((uint32_t*) 0x50004000));
// 	printf("DMA IntTCStat: %lu\n", *((uint32_t*) 0x50004004));
// 	printf("DMA IntErrStat: %lu\n", *((uint32_t*) 0x5000400C));
	
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

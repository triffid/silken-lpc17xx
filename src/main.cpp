
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

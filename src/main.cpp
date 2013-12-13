
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
	
	volatile uint32_t r;
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
		i = 10;

		for (;;)
		{
			printf("- %d\n", i);
			setleds(leds, i++);
			i &= 31;
			for (r = 0; r < (1<<20); r++);
		}
	}
	
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
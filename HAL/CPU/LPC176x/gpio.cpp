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

#include "gpio.h"

extern "C" {
	#include "LPC17xx.h"
	#include "lpc17xx_pinsel.h"
	#include "lpc17xx_gpio.h"
}

GPIO::GPIO(PinName pin)
{
	this->pin = pin;
	setup();
}

GPIO* GPIO::setup()
{
	PINSEL_CFG_Type PinCfg;
	
	PinCfg.Portnum   = PORT(pin);
	PinCfg.Pinnum    = PIN(pin);
	
	PinCfg.Funcnum   = 0;
	PinCfg.Pinmode   = PINSEL_PINMODE_PULLUP;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	
	PINSEL_ConfigPin(&PinCfg);

	return this;
}

GPIO* GPIO::set_direction(uint8_t direction)
{
	FIO_SetDir(PORT(pin), 1UL << PIN(pin), direction);

	return this;
}

GPIO* GPIO::output()
{
	set_direction(1);

	return this;
}

GPIO* GPIO::input()
{
	set_direction(0);

	return this;
}

GPIO* GPIO::write(uint8_t value)
{
	output();
	if (value)
		set();
	else
		clear();

	return this;
}

GPIO* GPIO::set()
{
	FIO_SetValue(PORT(pin), 1UL << PIN(pin));

	return this;
}

GPIO* GPIO::clear()
{
	FIO_ClearValue(PORT(pin), 1UL << PIN(pin));

	return this;
}

uint8_t GPIO::get()
{
	return (FIO_ReadValue(PORT(pin)) & (1UL << PIN(pin)))?255:0;
}

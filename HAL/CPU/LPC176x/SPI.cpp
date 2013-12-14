#include "SPI.h"

#include <cstdlib>

#include "lpc17xx_ssp.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_clkpwr.h"

#include "gpio.h"
#include "pins_platform.h"

#include "mri.h"

#define uabs(a, b) (((a) >= (b))?((a) - (b)):((b) - (a)))

struct _spi_platform_data
{
	GPIO* ss;
	
	union {
		uint8_t ssp_port;
		struct {
			uint8_t ssp_index:1;
			uint8_t ssp_alternate:7;
		};
	};
	
	LPC_SSP_TypeDef* ssp;
};

SPI::SPI(PinName mosi, PinName miso, PinName sck, PinName ss)
{
	data = (spi_platform_data*) malloc(sizeof(spi_platform_data));
	
	data->ss = (new GPIO(ss))->output()->set();
	
	data->ssp_port = 0;
	
	PINSEL_CFG_Type pin;
	
	if (mosi == SSP1_MOSI && miso == SSP1_MISO && sck == SSP1_SCK)
	{
		data->ssp_index = 1;
		
		pin.Portnum = mosi >> 5;
		pin.Pinnum  = mosi & 0x1F;
		pin.Funcnum = PINSEL_FUNC_2;
		PINSEL_ConfigPin(&pin);
		
		pin.Pinnum = miso & 0x1F;
		PINSEL_ConfigPin(&pin);
		
		pin.Pinnum = sck & 0x1F;
		PINSEL_ConfigPin(&pin);
	}
	else if (mosi == SSP0_MOSI && miso == SSP0_MISO && sck == SSP0_SCK)
	{
		data->ssp_index = 0;
	}
	else if (mosi == SSP0_ALT0_MOSI && miso == SSP0_ALT0_MISO && sck == SSP0_ALT0_SCK)
	{
		data->ssp_index = 0;
		data->ssp_alternate = 1;
	}
	else
	{
		// TODO: not a hardware SSP port, invoke soft-ssp
		__debugbreak();
	}
	
	switch(data->ssp_index)
	{
		case 0:
			data->ssp = LPC_SSP0;
			CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_SSP0, CLKPWR_PCLKSEL_CCLK_DIV_1);
			CLKPWR_ConfigPPWR(CLKPWR_PCONP_PCSSP0, ENABLE);
			break;
		case 1:
			data->ssp = LPC_SSP1;
			CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_SSP1, CLKPWR_PCLKSEL_CCLK_DIV_1);
			CLKPWR_ConfigPPWR(CLKPWR_PCONP_PCSSP1, ENABLE);
			break;
	}
	
	data->ssp->CR0   = SSP_DATABIT_8;
	data->ssp->CR1   = SSP_FRAME_SPI | SSP_MASTER_MODE;
	data->ssp->CPSR  = 2;
	data->ssp->IMSC  = 0;
	data->ssp->ICR   = SSP_ICR_ROR | SSP_ICR_RT;
	data->ssp->DMACR = 0;
	
	dma_locked = false;
	
	// nice slow freq by default
	set_frequency(10000);
}

void SPI::set_frequency(uint32_t f)
{
	// frequency = PCLK / (CPSR * (SCR + 1))
	uint32_t pclk = CLKPWR_GetPCLKSEL(data->ssp_index?CLKPWR_PCLKSEL_SSP1:CLKPWR_PCLKSEL_SSP0);
	
	/*
	 * given p and f, solve f = p / [2b * (c + 1)] for b and c, maximising b
	 * constraints: 1 <= b <= 127, 0 <= c <= 255
	 * 2b * (c + 1) = p / f
	 *  b * (c + 1) = p / 2f
	 *      (c + 1) = p / 2bf
	 *       c      = p / 2bf - 1
	 * 
	 * so now we can search b:[127..1], find c for each b then compute actual frequency and compare to desired
	 * 
	 */
	
	int best_b = 0;
	uint32_t best_f_delta = pclk + f;
	
	for (int b = 127; b; b--)
	{
		int s = pclk / b / f;
		
		// round s up or down for better accuracy
		if (s & 1) s++;
		s /= 2;
		
		if (s >= 1 && s <= 256)
		{
			uint32_t actual_f = pclk / b / s / 2;
			
			if (uabs(actual_f, f) < best_f_delta)
			{
				best_b = b;
				best_f_delta = uabs(actual_f, f);
				
				if (f == actual_f)
					break;
			}
		}
	}
	
	int s = pclk / best_b / f;
	
	if (s & 1) s++;
	s /= 2;
	
	data->ssp->CR0 = (data->ssp->CR0 & ~(0xFF << 8)) | (s - 1) << 8;
	data->ssp->CPSR = 2 * best_b;
	
	return;
}

uint8_t SPI::transfer(uint8_t out)
{
	while (dma_locked || (data->ssp->SR & SSP_SR_BSY))
		__WFI();
	
	data->ssp->DR = out;
	return data->ssp->DR;
}

void SPI::transfer_block(const uint8_t* tx, uint8_t* rx, int length)
{
	while (dma_locked || (data->ssp->SR & SSP_SR_BSY))
		__WFI();
	
	int dummy __attribute__ ((unused));
	while (data->ssp->SR & SSP_SR_RNE)
		dummy = data->ssp->DR;
	
	if (rx == NULL)
	{
		send_block(tx, length);
		return;
	}
	
	int i = 0;
	for (i = 0; i < length;)
	{
		int j = 0, k = 0;
		while (data->ssp->SR & SSP_SR_TNF)
			data->ssp->DR = tx[i + j++];
		while (data->ssp->SR & SSP_SR_RNE)
			rx[i + k++] = data->ssp->DR;
		if (j != k)
			__debugbreak();
		
		i += j;
	}
}

void SPI::send_block(const uint8_t* tx, int length)
{
	while (dma_locked || (data->ssp->SR & SSP_SR_BSY))
		__WFI();
	
	int dummy __attribute__ ((unused));
	while (data->ssp->SR & SSP_SR_RNE)
		dummy = data->ssp->DR;
	
	int i = 0;
	for (i = 0; i < length;)
	{
		int j = 0, k = 0;
		while (data->ssp->SR & SSP_SR_TNF)
			data->ssp->DR = tx[i + j++];
		while (data->ssp->SR & SSP_SR_RNE)
		{
			dummy = data->ssp->DR;
			k++;
		}
		if (j != k)
			__debugbreak();
		
		i += j;
	}
}

void SPI::recv_block(uint8_t* rx, int length, uint8_t txchar)
{
	while (dma_locked || (data->ssp->SR & SSP_SR_BSY))
		__WFI();
	
	int dummy __attribute__ ((unused));
	while (data->ssp->SR & SSP_SR_RNE)
		dummy = data->ssp->DR;
	
	int i = 0;
	for (i = 0; i < length;)
	{
		int j = 0, k = 0;
		while (data->ssp->SR & SSP_SR_TNF)
		{
			data->ssp->DR = txchar;
			j++;
		}
		while (data->ssp->SR & SSP_SR_RNE)
			rx[i + k++] = data->ssp->DR;
		if (j != k)
			__debugbreak();
		
		i += j;
	}
}

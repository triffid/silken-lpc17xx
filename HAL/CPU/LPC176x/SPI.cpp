#include "SPI.h"

#include <cstdlib>

#include "lpc17xx_ssp.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_clkpwr.h"

#include "gpio.h"
#include "pins_platform.h"

#include "min-printf.h"

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
	
	volatile uint32_t dummy;
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
	set_frequency(400000);

	data->ssp->CR1 |=  SSP_CR1_SSP_EN;
}

void SPI::set_frequency(uint32_t f)
{
	// frequency = PCLK / (CPSR * (SCR + 1))
	uint32_t pclk = CLKPWR_GetPCLK(data->ssp_index?CLKPWR_PCLKSEL_SSP1:CLKPWR_PCLKSEL_SSP0);

	while (data->ssp->SR & SSP_SR_BSY);
	while (data->ssp->SR & SSP_SR_RNE)
		data->dummy = data->ssp->DR;
	
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
	
	int best_b = 127;
	uint32_t best_f_delta = pclk;
	
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
	
	if (s < 1)
		s = 1;
	if (s > 256)
		s = 256;
	
	data->ssp->CR0 = (data->ssp->CR0 & ~(0xFF00)) | (s - 1) << 8;
	data->ssp->CPSR = 2 * best_b;
	
	return;
}

uint8_t SPI::transfer(uint8_t out)
{
	while (dma_locked || (data->ssp->SR & SSP_SR_BSY))
		__WFI();
	
	data->ssp->DR = out;
	
	while (data->ssp->SR & SSP_SR_BSY);
	
	return data->ssp->DR;
}

void SPI::transfer_block(const uint8_t* tx, uint8_t* rx, int length)
{
	while (dma_locked || (data->ssp->SR & SSP_SR_BSY))
		__WFI();
	
	while (data->ssp->SR & SSP_SR_RNE)
		data->dummy = data->ssp->DR;
	
	if (rx == NULL)
	{
		send_block(tx, length);
		return;
	}
	
	int i = 0;
	for (i = 0; i < length;)
	{
		int j = 0, k = 0;
		while ((data->ssp->SR & SSP_SR_TNF) && (i + j < length) && (j < 8))
			data->ssp->DR = tx[i + j++];
		while (data->ssp->SR & SSP_SR_BSY);
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
	
	while (data->ssp->SR & SSP_SR_RNE)
		data->dummy = data->ssp->DR;
	
	int i = 0;
// 	for (i = 0; i < length;)
// 	{
// 		int j = 0, k = 0;
// 		while ((data->ssp->SR & SSP_SR_TNF) && ((i + j) < length) && (j < 8))
// 			data->ssp->DR = tx[i + j++];
// 		while (data->ssp->SR & SSP_SR_BSY);
// 		while (data->ssp->SR & SSP_SR_RNE)
// 			data->dummy = data->ssp->DR;
// 		
// 		i += j;
// 	}
	while (i < length)
	{
		while ((data->ssp->SR & SSP_SR_TNF) && (i < length))
			data->ssp->DR = tx[i++];
		while (data->ssp->SR & SSP_SR_RNE)
			data->dummy = data->ssp->DR;
	}
	while ((data->ssp->SR & (SSP_SR_BSY | SSP_SR_TFE)) != SSP_SR_TFE);
	while (data->ssp->SR & SSP_SR_RNE)
		data->dummy = data->ssp->DR;
}

void SPI::recv_block(uint8_t* rx, int length, uint8_t txchar)
{
	while (dma_locked || (data->ssp->SR & SSP_SR_BSY))
		__WFI();
	
	while (data->ssp->SR & SSP_SR_RNE)
		data->dummy = data->ssp->DR;
	
	int i = 0;
	for (i = 0; i < length;)
	{
		int j = 0, k = 0;
		while ((data->ssp->SR & SSP_SR_TNF) && (i + j < length) && (j < 8))
		{
			data->ssp->DR = txchar;
			j++;
		}
		while (data->ssp->SR & SSP_SR_BSY);
		while (data->ssp->SR & SSP_SR_RNE)
			rx[i + k++] = data->ssp->DR;
		if (j != k)
			__debugbreak();
		
		i += j;
	}
}

void SPI::dma_begin(DMA* dma, dma_direction_t direction)
{
	if (direction == DMA_SENDER)
		data->ssp->DMACR |= SSP_DMA_RXDMA_EN;
	else
		data->ssp->DMACR |= SSP_DMA_TXDMA_EN;
}

void SPI::dma_complete(DMA* dma, dma_direction_t direction)
{
	if (direction == DMA_SENDER)
		data->ssp->DMACR &= ~SSP_DMA_RXDMA_EN;
	else
		data->ssp->DMACR &= ~SSP_DMA_TXDMA_EN;
}

void SPI::dma_configure(dma_config* config)
{
	config->mem_or_peripheral = DMA_PERIPHERAL;
	
	if      ((config->direction == DMA_SENDER  ) && (data->ssp_index == 0))
		config->peripheral_index = 1; // SSP0 RX, from UM10360 table 543
	else if ((config->direction == DMA_RECEIVER) && (data->ssp_index == 0))
		config->peripheral_index = 0; // SSP0 TX
	else if ((config->direction == DMA_SENDER  ) && (data->ssp_index == 1))
		config->peripheral_index = 3; // SSP1 RX
	else if ((config->direction == DMA_RECEIVER) && (data->ssp_index == 1))
		config->peripheral_index = 2; // SSP1 TX
	else
		__debugbreak();
	
	config->mem_buf = (void*) &data->ssp->DR;
	config->endianness = DMA_LITTLE_ENDIAN;
	config->word_size = DMA_WS_8BIT;
	config->burst_size = DMA_BS_8;
}

void SPI::begin_transaction(void)
{
	data->ss->clear();
}

void SPI::end_transaction(void)
{
	data->ss->set();
}

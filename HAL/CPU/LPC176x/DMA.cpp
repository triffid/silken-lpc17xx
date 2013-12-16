#include "DMA.h"

#include <cstdlib>

#include "LPC17xx.h"
#include "lpc17xx_clkpwr.h"
#include "lpc17xx_gpdma.h"

#include "min-printf.h"
#include "mri.h"

// from lpc17xx_gpdma.c
extern const LPC_GPDMACH_TypeDef *pGPDMACh[8];

struct _dma_impl
{
	int dma_channel;
	
	DMA_receiver* source;
	DMA_receiver* destination;
};

static volatile uint8_t dma_claimed_channels = 0;

static volatile DMA* channel_map[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

extern "C" {
	void DMA_IRQHandler() __attribute__ ((isr));
	void DMA_IRQHandler()
	{
		for (int i = 0; i < 8; i++)
		{
			if (LPC_GPDMA->DMACIntStat & (1<<i))
			{
				if (channel_map[i])
				{
					DMA* dma = (DMA*) channel_map[i];
					dma->isr();
				}
				else
				{
					LPC_GPDMA->DMACIntTCClear = 1<<i;
					LPC_GPDMA->DMACIntErrClr  = 1<<i;
				}
			}
		}
	}
}

DMA::DMA()
{
	data = (dma_impl*) malloc(sizeof(dma_impl));
	data->dma_channel = -1;
	
	data->source = NULL;
	data->destination = NULL;
}

DMA::DMA(DMA_receiver* source, DMA_receiver* dest)
{
	data = (dma_impl*) malloc(sizeof(dma_impl));
	data->dma_channel = -1;
	
	set_source(source);
	set_destination(dest);
}

void DMA::set_source(DMA_receiver* source)
{
	data->source = source;
}

void DMA::set_destination(DMA_receiver* destination)
{
	data->destination = destination;
}

void DMA::setup(uint32_t size)
{
	CLKPWR_ConfigPPWR(CLKPWR_PCONP_PCGPDMA, ENABLE);
	
	NVIC_EnableIRQ(DMA_IRQn);
	
	if (data->dma_channel < 0)
	{
		__disable_irq();
		for (int i = 7; i >= 0; i--)
		{
			if (channel_map[i] == NULL)
			{
				data->dma_channel = i;
				break;
			}
		}
		__enable_irq();
	}

	channel_map[data->dma_channel] = this;
	
	GPDMA_Channel_CFG_Type chconfig;
	
	chconfig.ChannelNum = data->dma_channel;
	chconfig.TransferSize = size;
	chconfig.TransferWidth = GPDMA_WIDTH_BYTE;
	chconfig.DMALLI = NULL;
	
	dma_config sconfig, dconfig;
	
	sconfig.direction = DMA_SENDER;
	data->source->dma_configure(&sconfig);
	dconfig.direction = DMA_RECEIVER;
	data->destination->dma_configure(&dconfig);
	
	chconfig.SrcMemAddr = (uint32_t) sconfig.mem_buf;
	chconfig.SrcConn    = sconfig.peripheral_index;
	
	chconfig.DstMemAddr = (uint32_t) dconfig.mem_buf;
	chconfig.DstConn    = dconfig.peripheral_index;
	
	if (sconfig.mem_or_peripheral == DMA_MEM && dconfig.mem_or_peripheral == DMA_MEM)
		chconfig.TransferType = GPDMA_TRANSFERTYPE_M2M;
	else if (sconfig.mem_or_peripheral == DMA_MEM && dconfig.mem_or_peripheral == DMA_PERIPHERAL)
		chconfig.TransferType = GPDMA_TRANSFERTYPE_M2P;
	else if (sconfig.mem_or_peripheral == DMA_PERIPHERAL && dconfig.mem_or_peripheral == DMA_MEM)
		chconfig.TransferType = GPDMA_TRANSFERTYPE_P2M;
	else if (sconfig.mem_or_peripheral == DMA_PERIPHERAL && dconfig.mem_or_peripheral == DMA_PERIPHERAL)
		chconfig.TransferType = GPDMA_TRANSFERTYPE_P2P;
	
	GPDMA_Setup(&chconfig);
	
	LPC_GPDMACH_TypeDef *pDMAch = (LPC_GPDMACH_TypeDef*) pGPDMACh[data->dma_channel];
	
	uint32_t control = pDMAch->DMACCControl;
	
	if (sconfig.mem_or_peripheral == DMA_MEM)
	{
		// SBsize = 0
		control = (control & ~(7<<12)) | (0<<12);
		// Swidth = 32b
		control = (control & ~(7<<18)) | (2<<18);
		
		if (sconfig.auto_increment == DMA_NO_INCREMENT)
			control &= ~(GPDMA_DMACCxControl_SI);

		control = (control & (~2047)) | ((size / 4) & 2047);
	}
	else
		control = (control & (~2047)) | (size & 2047);
	
	if (dconfig.mem_or_peripheral == DMA_MEM)
	{
		// DBsize = 1
		control = (control & ~(7<<15)) | (1<<15);
		// Dwidth = 32b
		control = (control & ~(7<<21)) | (2<<21);
		
		if (dconfig.mem_or_peripheral == DMA_MEM && dconfig.auto_increment == DMA_NO_INCREMENT)
			control &= ~(GPDMA_DMACCxControl_DI);
	}

	pDMAch->DMACCControl = control;
	
// 	debug();
}

void DMA::begin()
{
	data->source->dma_begin(this, DMA_SENDER);
	data->destination->dma_begin(this, DMA_RECEIVER);
	
// 	printf("DMACH %d Begin!\n", data->dma_channel);

	LPC_GPDMACH_TypeDef *pDMAch = (LPC_GPDMACH_TypeDef*) pGPDMACh[data->dma_channel];
	
	do {
		pDMAch->DMACCConfig |= GPDMA_DMACCxConfig_E;
	} while ((pDMAch->DMACCConfig & GPDMA_DMACCxConfig_E) == 0);
}

int DMA::running()
{
	LPC_GPDMACH_TypeDef *pDMAch = (LPC_GPDMACH_TypeDef*) pGPDMACh[data->dma_channel];
	
	if (data->dma_channel >= 0 && data->dma_channel <= 7)
	{
		if (pDMAch->DMACCConfig & GPDMA_DMACCxConfig_A)
			return pDMAch->DMACCControl & 2047; // TransferSize
		if (pDMAch->DMACCControl & 2047)
			return -1;
	}
	return 0;
}

void DMA::isr()
{
	LPC_GPDMA->DMACIntTCClear = (1 << data->dma_channel);
	LPC_GPDMA->DMACIntErrClr  = (1 << data->dma_channel);

	data->source->dma_complete(this, DMA_SENDER);
	data->destination->dma_complete(this, DMA_RECEIVER);
	
	channel_map[data->dma_channel] = NULL;
}

void DMA::debug()
{
	if (data->dma_channel < 0 || data->dma_channel > 7)
	{
		printf("No Channel: %d\n", data->dma_channel);
		return;
	}
	
	LPC_GPDMACH_TypeDef *pDMAch = (LPC_GPDMACH_TypeDef*) pGPDMACh[data->dma_channel];
	
	printf("*** DMA Channel: %d\n", data->dma_channel);
	
	printf("\tIntStat: %lu TCStat: %lu ErrStat: %lu\n",
		LPC_GPDMA->DMACIntStat >> data->dma_channel & 1,
		LPC_GPDMA->DMACIntTCStat >> data->dma_channel & 1,
		LPC_GPDMA->DMACIntErrStat >> data->dma_channel & 1
	);
	
	uint32_t _d_control = pDMAch->DMACCControl;
	printf("\tDMA Control (%p):\n\t\tTransferSize: %lu\n\t\tSBSize: %lu (0=1,1=4,2=8,3=16,...)\n\t\tDBSize: %lu (0=1,1=4,2=8,3=16,...)\n\t\tSWidth: %lu (0=8,1=16,2=32)\n\t\tDWidth: %lu (0=8,1=16,2=32)\n\t\tSI: %lu\n\t\tDI: %lu\n\t\tI: %lu\n",
		&pDMAch->DMACCControl,
		_d_control & 2047,
		_d_control >> 12 & 7,
		_d_control >> 15 & 7,
		_d_control >> 18 & 7,
		_d_control >> 21 & 7,
		_d_control >> 26 & 1,
		_d_control >> 27 & 1,
		_d_control >> 31 & 1
	);
	
	uint32_t _d_config = pDMAch->DMACCConfig;
	printf("\tDMA Config (%p):\n\t\tE: %lu\n\t\tSrcPeripheral: %lu\n\t\tDestPeripheral: %lu\n\t\tTransferType: %lu (0=M2M,1=M2P,2=P2M,3=P2P)\n\t\tIE: %lu\n\t\tITC: %lu\n\t\tActive: %lu\n\t\tHalt: %lu\n",
		&pDMAch->DMACCConfig,
		_d_config & 1,
		_d_config >> 1 & 31,
		_d_config >> 6 & 31,
		_d_config >> 11 & 7,
		_d_config >> 14 & 1,
		_d_config >> 15 & 1,
		_d_config >> 17 & 1,
		_d_config >> 18 & 1
	);
	
	printf("\tDMA SrcAddr: %p\n", (void*) pDMAch->DMACCSrcAddr);
	printf("\tDMA DstAddr: %p\n", (void*) pDMAch->DMACCDestAddr);
	
}

/*
 * DMA_mem
 */

void DMA_mem::dma_configure(dma_config* config)
{
	config->mem_or_peripheral = DMA_MEM;
	config->mem_buf = addr;
	config->mem_size = size;
	config->endianness = DMA_BIG_ENDIAN;
	config->word_size = DMA_WS_32BIT;
	config->burst_size = DMA_BS_128;
	config->auto_increment = auto_increment;
}

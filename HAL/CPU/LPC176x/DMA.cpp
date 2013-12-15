#include "DMA.h"

#include <cstdlib>

#include "LPC17xx.h"
#include "lpc17xx_gpdma.h"

struct _dma_impl
{
	int dma_channel;
	
	DMA_receiver* source;
	DMA_receiver* destination;
};

static volatile uint8_t dma_claimed_channels = 0;

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

void DMA::begin(uint32_t size)
{
	if (data->dma_channel < 0)
	{
		__disable_irq();
		for (int i = 7; i >= 0; i--)
		{
			if ((dma_claimed_channels & (1<<i)) == 0)
			{
				data->dma_channel = i;
				dma_claimed_channels |= (1<<i);
				break;
			}
		}
		__enable_irq();
	}
		
	GPDMA_Channel_CFG_Type chconfig;
	
	chconfig.ChannelNum = data->dma_channel;
	chconfig.TransferSize = size;
	chconfig.TransferWidth = GPDMA_WIDTH_WORD;
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
	
	// turn off auto-increment
	if (sconfig.mem_or_peripheral == DMA_MEM && sconfig.auto_increment == DMA_NO_INCREMENT)
		LPC_GPDMACH0[data->dma_channel].DMACCConfig &= ~(GPDMA_DMACCxControl_SI);
	if (dconfig.mem_or_peripheral == DMA_MEM && dconfig.auto_increment == DMA_NO_INCREMENT)
		LPC_GPDMACH0[data->dma_channel].DMACCConfig &= ~(GPDMA_DMACCxControl_DI);
	
	// TODO
	
	data->source->dma_begin(this);
	data->destination->dma_begin(this);
	
	// TODO
}

bool DMA::running()
{
	if (data->dma_channel >= 0 && data->dma_channel <= 7)
		return LPC_GPDMACH0[data->dma_channel].DMACCConfig & GPDMA_DMACCxConfig_A;
	return false;
}

void DMA::isr()
{
	// TODO
	
	data->source->dma_complete(this);
	data->destination->dma_complete(this);
}

void DMA_mem::dma_configure(dma_config* config)
{
	config->mem_or_peripheral = DMA_MEM;
	config->mem_buf = addr;
	config->mem_size = size;
	config->endianness = DMA_BIG_ENDIAN;
	config->word_size = DMA_WS_32BIT;
	config->burst_size = DMA_BS_128;
}
#include "DMA.h"

#include <cstdint>
#include <cstdlib>

struct _dma_impl
{
	int dma_channel;
	
	DMA_receiver* source;
	DMA_receiver* destination;
};

struct _dma_config
{
	enum {
		DMA_MEM,
		DMA_PERIPHERAL
	} mem_or_peripheral;
	
	union {
		int peripheral_index;
		int mem_size;
	};
	
	void* mem_buf;
	
	enum {
		DMA_LITTLE_ENDIAN,
		DMA_BIG_ENDIAN
	} endianness;
	
	uint8_t word_size;
	uint8_t burst_size;
};

DMA::DMA()
{
	data = (dma_impl*) malloc(sizeof(dma_impl));
	data->dma_channel = -1;
	data->source = NULL;
	data->destination = NULL;
}

void DMA::set_source(DMA_receiver* source)
{
	dma_config config;
	source->dma_configure(&config);
	// TODO
	data->source = source;
}

void DMA::set_destination(DMA_receiver* destination)
{
	dma_config config;
	destination->dma_configure(&config);
	// TODO
	data->destination = destination;
}

void DMA::begin()
{
	data->source->dma_begin(this);
	data->destination->dma_begin(this);
	
	// TODO
}

bool DMA::running()
{
	// TODO
}

void DMA::isr()
{
	// TODO
	
	data->source->dma_complete(this);
	data->destination->dma_complete(this);
}

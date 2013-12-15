#ifndef _DMA_PLATFORM_H
#define _DMA_PLATFORM_H

#include <cstdint>

typedef enum {
	DMA_SENDER,
	DMA_RECEIVER
} dma_direction_t;

typedef enum {
	DMA_MEM,
	DMA_PERIPHERAL
} dma_type_t;

typedef enum {
	DMA_WS_8BIT,
	DMA_WS_16BIT,
	DMA_WS_32BIT
} dma_wordsize_t;

typedef enum {
	DMA_BS_1,
	DMA_BS_4,
	DMA_BS_8,
	DMA_BS_16,
	DMA_BS_32,
	DMA_BS_64,
	DMA_BS_128,
	DMA_BS_256
} dma_burstsize_t;

typedef enum {
	DMA_AUTO_INCREMENT,
	DMA_NO_INCREMENT
} dma_auto_increment_t;

typedef enum {
	DMA_LITTLE_ENDIAN,
	DMA_BIG_ENDIAN
} dma_endianness_t;
struct _dma_config
{
	dma_direction_t      direction;
	dma_type_t           mem_or_peripheral;
	
	union {
		int      peripheral_index;
		uint32_t mem_size;
	};
	
	void* mem_buf;
	
	dma_auto_increment_t auto_increment;
	dma_endianness_t     endianness;
	dma_wordsize_t       word_size;
	dma_burstsize_t      burst_size;
};

#endif /* _DMA_PLATFORM_H */

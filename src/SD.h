#ifndef _SD_H
#define _SD_H

#include "SPI.h"

typedef enum {
	SD_TYPE_NONE,
	SD_TYPE_MMC,
	SD_TYPE_SD,
	SD_TYPE_SDHC
} SD_CARD_TYPE;

typedef enum {
	SD_WORK_ACTION_NONE,

	SD_WORK_ACTION_INIT,

	SD_WORK_ACTION_READ,
	SD_WORK_ACTION_WRITE
} SD_WORK_ACTION;

class SD;

class SD_async_receiver {
public:
	virtual void sd_read_complete(SD*, uint32_t sector, void* buf, int err) = 0;
	virtual void sd_write_complete(SD*, uint32_t sector, void* buf, int err) = 0;
};

struct _sd_work_stack;
typedef struct _sd_work_stack sd_work_stack_t;

struct _sd_work_stack {
	SD_WORK_ACTION action;

	uint32_t sector;
	void*    buf;
	uint8_t  status;

	SD_async_receiver* receiver;

	sd_work_stack_t*   next;
};

class SD : public DMA_receiver
{
public:
	SD(SPI*);

	void on_idle(void);
	
	int init();
	
	uint32_t n_sectors(void);
	
// 	int read(uint32_t sector, void* buf);
// 	int write(uint32_t sector, void* buf);
//
// 	int read_multi(uint32_t sector, void* buf, int sectors);
// 	int write_multi(uint32_t sector, void* buf, int sectors);
// 	int pre_erase(uint32_t sector, uint32_t n_sectors);
	
	int begin_read(uint32_t sector, void* buf, SD_async_receiver*);
	int begin_write(uint32_t sector, void* buf, SD_async_receiver*);

	SD_CARD_TYPE get_type(void);

	/*
	 * implementation of DMA_receiver
	 */
	void dma_begin(DMA*, dma_direction_t);
	void dma_complete(DMA*, dma_direction_t);
	void dma_configure(dma_config*);

	void work_stack_work(void);
	void work_stack_debug(void);
protected:
	SPI* spi;
	
	SD_CARD_TYPE card_type;
	uint32_t sector_count;

	DMA_mem dma_rxmem;
	DMA_mem dma_txmem;

	DMA dma_tx;
	DMA dma_rx;

	uint32_t txm;

	sd_work_stack_t* work_stack;

	// garbage collector stack
	// we marshall completed items here so they can be freed outside of interrupt context
	sd_work_stack_t* gc_stack;

	void work_stack_pop();

	volatile uint8_t work_flags;
};

#endif /* _SD_H */

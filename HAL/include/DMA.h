#ifndef _DMA_H
#define _DMA_H

#include <cstdint>

/*
 * direction flag
 */
typedef enum {
	DMA_SENDER,
	DMA_RECEIVER
} dma_direction_t;

/*
 * auto increment flag
 */
typedef enum {
	DMA_AUTO_INCREMENT,
	DMA_NO_INCREMENT
} dma_auto_increment_t;

/*
 * predeclarations
 */

class DMA;
class DMA_receiver;

/*
 * configuration struct
 * 
 * implementation is cpu-dependent
 */

#include "DMA_platform.h"
typedef struct _dma_config dma_config;

/*
 * DMA implementation metadata struct
 * 
 * implementation is cpu dependent
 */

struct _dma_impl;
typedef struct _dma_impl dma_impl;

/*
 * receiver interface class
 */

class DMA_receiver
{
public:
	virtual void dma_begin(DMA*, dma_direction_t) = 0;
	
	virtual void dma_complete(DMA*, dma_direction_t) = 0;
	
	virtual void dma_configure(dma_config*) = 0;
	
	volatile bool dma_locked;
};

/*
 * class to handle a specific DMA channel
 */

class DMA
{
public:
	DMA();
	DMA(DMA_receiver* source, DMA_receiver* dest);
	
	void set_source(DMA_receiver*);
	void set_destination(DMA_receiver*);
	
	void setup(uint32_t size);
	void begin();
	
	int  running(void);
	
	void isr(void);
	
	void debug(void);

private:
	dma_impl* data;
};

class DMA_mem : public DMA_receiver
{
public:
	DMA_mem(void)
	{
		addr = (void*)0;
		size = 0;
		auto_increment = DMA_AUTO_INCREMENT;
	}
	
	DMA_mem(void* addr, uint32_t size)
	{
		setup(addr, size);
	};
	
	void setup(void* addr, uint32_t size)
	{
		this->addr = addr;
		this->size = size;
		this->auto_increment = DMA_AUTO_INCREMENT;
	};
	
	void dma_begin(DMA*, dma_direction_t) {};
	void dma_complete(DMA*, dma_direction_t) {};
	void dma_configure(dma_config*);
	
	void* addr;
	uint32_t size;
	dma_auto_increment_t auto_increment;
}
;
#endif /* _DMA_H */

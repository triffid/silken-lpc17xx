#ifndef _DMA_H
#define _DMA_H

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

struct _dma_config;
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
	virtual void dma_begin(DMA*) = 0;
	virtual void dma_complete(DMA*) = 0;
	
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
	
	void set_source(DMA_receiver*);
	void set_destination(DMA_receiver*);
	
	void begin(void);
	
	bool running(void);
	
	void isr(void);

private:
	dma_impl* data;
};

#endif /* _DMA_H */

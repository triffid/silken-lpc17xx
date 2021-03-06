#ifndef _SPI_H
#define _SPI_H

#include <cstdint>

#include "pins.h"
#include "DMA.h"

struct _spi_platform_data;
typedef struct _spi_platform_data spi_platform_data;

class SPI : public DMA_receiver
{
public:
	SPI(PinName mosi, PinName miso, PinName sck, PinName ss);
	
	void    set_frequency(uint32_t frequency);
	
	void    begin_transaction(void);
	void    end_transaction(void);
	
	uint8_t transfer(uint8_t data);
	
	void    transfer_block(const uint8_t* tx, uint8_t* rx, int length);
	
	void    send_block(const uint8_t* tx, int length) __attribute__ ((optimize("O0")));
	void    recv_block(      uint8_t* rx, int length, uint8_t txchar);
	
	// implementation of DMA_receiver
	void    dma_begin(DMA*, dma_direction_t);
	void    dma_complete(DMA*, dma_direction_t);
	void    dma_configure(dma_config*);
private:
	spi_platform_data* data;
};

#endif /* _SPI_H */

#include "SPI.h"

SPI::SPI(PinName mosi, PinName miso, PinName sck, PinName cs)
{
	dma_locked = false;
}

void SPI::set_frequency(uint32_t frequency)
{
}

uint8_t transfer(uint8_t data)
{
}

void transfer_block(const uint8_t* tx, uint8_t* rx, int length)
{
}

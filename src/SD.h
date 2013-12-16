#ifndef _SD_H
#define _SD_H

#include "SPI.h"

class SD_async_receiver {
public:
	virtual void sd_read_complete(SD*, uint32_t sector, void* buf) = 0;
	virtual void sd_write_complete(SD*, uint32_t sector, void* buf) = 0;
};

class SD
{
public:
	SD(SPI*);
	
	int init();
	
	uint32_t n_sectors(void);
	
	int read(uint32_t sector, void* buf);
	int write(uint32_t sector, void* buf);
	
	int read_multi(uint32_t sector, void* buf, int sectors);
	int write_multi(uint32_t sector, void* buf, int sectors);
	int pre_erase(uint32_t sector, uint32_t n_sectors);
	
	int begin_read(uint32_t sector, void* buf, SD_async_receiver*);
	int begin_write(uint32_t sector, void* buf, SD_async_receiver*);
	
protected:
	SPI* spi;
	
	enum SD_CARD_TYPE {
		SD_TYPE_NONE,
		SD_TYPE_MMC,
		SD_TYPE_SD,
		SD_TYPE_SDHC
	} card_type;
};

#endif /* _SD_H */

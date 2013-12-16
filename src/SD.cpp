#include "SD.h"

#define CMD_TIMEOUT 32
#define READ_TIMEOUT 512
#define WRITE_TIMEOUT 512

#define SD_HIGH_CAPACITY (1<<30)

#include "platform_utils.h"

typedef struct __attribute__ ((packed))
{
	uint8_t  reserved0          :1;
	uint8_t  crc                :7;
	
	uint8_t  reserved1          :2;
	uint8_t  file_format        :2;
	uint8_t  tmp_write_protect  :1;
	uint8_t  perm_write_protect :1;
	uint8_t  copy               :1;
	uint8_t  file_format_group  :1;
	
	uint16_t reserved2          :5;
	uint16_t write_bl_partial   :1;
	uint16_t write_bl_len       :4;
	uint16_t r2w_factor         :3;
	uint16_t reserved3          :2;
	uint16_t wp_grp_enable      :1;
	
	uint16_t wp_grp_size        :7;
	uint16_t sector_size        :7;
	uint16_t erase_blk_len      :1;
	uint16_t reserved4          :1;
	
	uint32_t c_size             :22;
	uint32_t reserved5          :6;
	uint32_t dsr_imp            :1;
	uint32_t read_blk_misalign  :1;
	uint32_t write_blk_misalign :1;
	uint32_t read_bl_partial    :1;
	
	uint16_t read_bl_len        :4;
	uint16_t ccc                :12;
	
	uint8_t  tran_speed         :8;
	uint8_t  nsac               :8;
	uint8_t  taac               :8;
	
	uint8_t  reserved6          :6;
	uint8_t  csd_structure      :2;
} CSD_v2_type;

typedef struct __attribute__ ((packed))
{
	uint8_t  reserved0          :1;
	uint8_t  crc                :7;
	
	uint8_t  reserved1          :2;
	uint8_t  file_format        :2;
	uint8_t  tmp_write_protect  :1;
	uint8_t  perm_write_protect :1;
	uint8_t  copy               :1;
	uint8_t  file_format_group  :1;
	
	uint16_t reserved2          :5;
	uint16_t write_bl_partial   :1;
	uint16_t write_bl_len       :4;
	uint16_t r2w_factor         :3;
	uint16_t reserved3          :2;
	uint16_t wp_grp_enable      :1;
	
	uint32_t wp_grp_size        :7;
	uint32_t sector_size        :7;
	uint32_t erase_blk_len      :1;
	uint32_t c_size_mult        :3;
	uint32_t vdd_w_curr_max     :3;
	uint32_t vdd_w_curr_min     :3;
	uint32_t vdd_r_curr_max     :3;
	uint32_t vdd_r_curr_min     :3;
	
	uint32_t c_size             :12;
	uint32_t reserved4          :2;
	
	uint32_t dsr_imp            :1;
	uint32_t read_blk_misalign  :1;
	uint32_t write_blk_misalign :1;
	uint32_t read_bl_partial    :1;
	
	uint16_t read_bl_len        :4;
	uint16_t ccc                :12;
	
	uint8_t  tran_speed         :8;
	uint8_t  nsac               :8;
	uint8_t  taac               :8;
	
	uint8_t  reserved6          :6;
	uint8_t  csd_structure      :2;
} CSD_v1_type;

typedef struct {
	uint8_t  reserved0          :1;
	uint8_t  crc                :7;
	
	uint16_t mdt                :12;
	uint16_t reserved1          :4;
	
	uint32_t psn;
	uint8_t  prv;
	uint8_t  pnm[5];
	uint16_t oid;
	uint8_t  mid;
} CID_type;

static int sd_cmdx(SPI* spi, int cmd, uint32_t arg)
{
	union {
		struct __attribute__ ((packed)) {
			uint32_t dummy;
			uint8_t cmd;
			uint32_t arg;
			uint8_t checksum;
		};
		uint8_t packet[10];
	} spi_cmd0;
	
	spi->begin_transaction();
	
	spi_cmd0.dummy = 0xFFFFFFFF;
	
	spi_cmd0.cmd = 0x40 | cmd;
	spi_cmd0.arg = htonl(arg);
	if (cmd == 8)
		spi_cmd0.checksum = 0x87;
	else
		spi_cmd0.checksum = 0x95;
	
	spi->send_block(spi_cmd0.packet, sizeof(spi_cmd0));
	
	int i;
	uint8_t r;
	for (i = 0; i < CMD_TIMEOUT; i++)
	{
		if (((r = spi->transfer(0xFF)) & 0x80) == 0)
			break;
	}
	
	if (i >= CMD_TIMEOUT)
		// error: cmd failed
		return -1;
	
	return r;
}

static int sd_response(SPI* spi, void* buf, int size)
{
	uint8_t* b = (uint8_t*) buf;
	
	int i = CMD_TIMEOUT;
	
	do {
		b[0] = spi->transfer(0xFF);
		
		if (--i <= 0)
			// CMD timeout
			return -1;
	}
	while (b[0] == 0xFF);
	
	spi->recv_block(b + 1, size - 1, 0xFF);
	
	return size;
}

static int sd_cmd(SPI* spi, int cmd, uint32_t arg)
{
	int r = sd_cmdx(spi, cmd, arg);
	spi->end_transaction();
	
	return r;
}

static int cmd0(SPI* spi)
{
	return sd_cmd(spi, 0, 0);
}

static int cmd8(SPI* spi)
{
	union {
		uint8_t b[4];
		uint32_t w;
	} buf;
	
	buf.w = 0;
	
	int r;
	
	if ((r = sd_cmdx(spi, 8, 0x1AA)) & 0x7A)
	{
		// comms error
		spi->end_transaction();
		return -1;
	}
	
	if (r & 4)
	{
		// illegal command- no data follows
// 		card_type = SD_TYPE_MMC;
		spi->end_transaction();
		return r;
	}

	// collect IF_COND register
	if (sd_response(spi, buf.b, 4) < 0)
		return -1;
	
	spi->end_transaction();
	
	if (ntohl(buf.w) != 0x1AA)
		return -1;
	
	return r;
}

static int cmd58(SPI* spi, uint32_t* ocr)
{
	union {
		uint8_t b[4];
		uint32_t ocr;
	} buf;
	
	int r;
	
	if ((r = sd_cmdx(spi, 58, 0)) != 1)
		return -1;
	
	if (sd_response(spi, buf.b, 4) < 0)
		return -1;
	
	spi->end_transaction();
	
	*ocr = ntohl(buf.ocr);
	
	return r;
}

static int acmd41(SPI* spi)
{
	int r;
	
	int i = READ_TIMEOUT;
	
	do
	{
		if ((r = sd_cmd(spi, 55, 0)) & 0x7E)
			return -1;
		
		if ((r = sd_cmd(spi, 41, SD_HIGH_CAPACITY)) & 0x7E)
			return -1;
		
		if (--i <= 0)
			return -1;
	}
	while (r == 1);
	
	return r;
}

static int cmd9(SPI* spi, void* CSD)
{
	int r;
	
	r = sd_cmdx(spi, 9, 0);
	if (r != 1)
		return -1;
	
	if (sd_response(spi, CSD, 16) < 0)
		return -1;
	
	spi->end_transaction();
	
	return r;
}

static int cmd10(SPI* spi, void* CID)
{
	int r;
	
	r = sd_cmdx(spi, 10, 0);
	if (r != 1)
		return -1;
	
	if (sd_response(spi, CID, 16) < 0)
		return -1;
	
	spi->end_transaction();
	
	return r;
}

SD::SD(SPI* spi)
{
	this->spi = spi;
	
	card_type = SD_TYPE_NONE;
}

int SD::init()
{
	int i, r;
	
	card_type = SD_TYPE_NONE;
	
	spi->set_frequency(25000);
	
	/*
	 * CMD0
	 */
	for (i = 0; i < CMD_TIMEOUT; i++)
	{
		r = cmd0(spi);
		if (r == 1)
			break;
	}
	
	if (i >= CMD_TIMEOUT)
		return -1;
	
	/*
	 * CMD8
	 */
	r = cmd8(spi);
	
	if (r & 4)
		card_type = SD_TYPE_MMC;
	
	// TODO: support MMC
	if (r != 1)
		return -1;
	
	/* 
	 * acmd41
	 */
	r = acmd41(spi);
	if (r != 0)
		return -1;
	
	/*
	 * CMD58
	 */
	uint32_t ocr;
	
	do
	{
		r = cmd58(spi, &ocr);
		
		if ((ocr & (1<<20)) == 0)
			// error: card does not support 3.2-3.3v!
			return -1;
		
		if (ocr & SD_HIGH_CAPACITY)
			card_type = SD_TYPE_SDHC;
		else
			card_type = SD_TYPE_SD;
	}
	while ((ocr & (1<<31)) == 0);
	
	// card is fully started, now boost frequency	
	spi->set_frequency(10000000);

	union {
		CSD_v1_type csd_v1;
		CSD_v2_type csd_v2;
		uint8_t b[16];
	} csd;
	
	cmd9(spi, &csd);
	
	union {
		CID_type cid;
		uint8_t b[16];
	} cid;
	
	cmd10(spi, &cid);
	
	return 1;
}

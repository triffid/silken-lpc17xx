#include "SD.h"

#include <cstddef>
#include <cstdlib>

#include "platform_utils.h"

#include "mri.h"
#include "min-printf.h"

#define CMD_TIMEOUT 32
#define READ_TIMEOUT 512
#define WRITE_TIMEOUT 512

enum {
	SD_READ_STATUS_START,
	SD_READ_WAIT_CMD_RESPONSE,
	SD_READ_STATUS_WAIT_TRAN,
	SD_READ_STATUS_DMA,
	SD_READ_STATUS_CHECKSUM
} SD_READ_STATUS;

enum {
	SD_WRITE_STATUS_START,
	SD_WRITE_WAIT_CMD_RESPONSE,
	SD_WRITE_STATUS_DMA,
	SD_WRITE_STATUS_WAIT_BSY
} SD_WRITE_STATUS;

typedef enum {
	SD_FLAG_IDLE     = 0,
	SD_FLAG_RUNNING  = 1,
	SD_FLAG_ERROR    = 2,
	SD_FLAG_WAIT_BSY = 4,
	SD_FLAG_REQ_WORK = 8
} SD_WORK_FLAGS;

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
	} spi_cmd;
	
	spi->begin_transaction();
	
	spi_cmd.dummy = 0xFFFFFFFF;
	
	spi_cmd.cmd = 0x40 | cmd;
	spi_cmd.arg = htonl(arg);
	if (cmd == 8)
		spi_cmd.checksum = 0x87;
	else
		spi_cmd.checksum = 0x95;

	printf("spi_cmdx Send: ");
	for (uint32_t q = 0; q < sizeof(spi_cmd); q++)
		printf("0x%X ", spi_cmd.packet[q]);
	printf("\n");
	
	spi->send_block(spi_cmd.packet, sizeof(spi_cmd));

	printf("         Recv: ");
	
	int i;
	uint8_t r;
	for (i = 0; i < CMD_TIMEOUT; i++)
	{
		if (((r = spi->transfer(0xFF)) & 0x80) == 0)
			break;
		printf("0x%X ", r);
	}
	printf("0x%X\n", r);

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

	printf("    RecvBlock: ");
	for (int q = 0; q < size; q++)
		printf("0x%X%c", b[q], ((q & 31) == 31)?'\n':' ');
	printf("\n");
	
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
	
	if ((r = sd_cmdx(spi, 58, 0)) & 0x7E)
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
	sector_count = 0;

	txm = 0xFFFFFFFF;
	dma_txmem.setup(&txm, 4);
	dma_txmem.auto_increment = DMA_NO_INCREMENT;

	dma_tx.set_source(&dma_txmem);
	dma_tx.set_destination(this);

	dma_rx.set_source(this);
	dma_rx.set_destination(&dma_rxmem);

	work_stack = NULL;

	work_flags = SD_FLAG_IDLE;
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
		return -2;
	
	/* 
	 * acmd41
	 */
	r = acmd41(spi);
	if (r != 0)
		return -3;
	
	/*
	 * CMD58
	 */
	uint32_t ocr;
	
// 	__debugbreak();
	do
	{
		r = cmd58(spi, &ocr);

		if (r & 0x7E)
			return -4;
		
		if ((ocr & (1<<20)) == 0)
			// error: card does not support 3.2-3.3v!
			return -5;
		
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

void SD::on_idle()
{
// 	printf("%d", work_flags & (SD_FLAG_RUNNING | SD_FLAG_REQ_WORK));
	if ((work_flags & (SD_FLAG_RUNNING | SD_FLAG_REQ_WORK)) == (SD_FLAG_RUNNING | SD_FLAG_REQ_WORK))
		work_stack_work();
}

SD_CARD_TYPE SD::get_type()
{
	return card_type;
}

uint32_t SD::n_sectors()
{
	return sector_count;
}

int SD::begin_read(uint32_t sector, void* buf, SD_async_receiver* receiver)
{
	sd_work_stack_t* w = (sd_work_stack_t*) malloc(sizeof(sd_work_stack_t));

	w->action   = SD_WORK_ACTION_READ;
	w->buf      = buf;
	w->sector   = sector;
	w->receiver = receiver;
	w->status   = SD_READ_STATUS_START;
	w->next     = NULL;

	sd_work_stack_t* x = work_stack;
	__disable_irq();
	if (x)
	{
		while (x->next)
			x = x->next;
		x->next = w;
		__enable_irq();
	}
	else
	{
		work_stack = w;
		__enable_irq();
		work_stack_work();
	}

	work_stack_debug();

	return 0;
}

void SD::work_stack_work(void)
{
	sd_work_stack_t* w = work_stack;

	switch(w->action)
	{
		case SD_WORK_ACTION_READ:
		{
			switch(w->status)
			{
				case SD_READ_STATUS_START:
				{
					work_flags |= SD_FLAG_RUNNING;

					uint32_t addr;
					if (card_type == SD_TYPE_SDHC)
						addr = w->sector;
					else if (card_type == SD_TYPE_SD)
						addr = w->sector << 9;
					else
					{
						work_flags |= SD_FLAG_ERROR;
						if (w->receiver)
							w->receiver->sd_read_complete(this, w->sector, w->buf, w->status + 1);
						// TODO: support MMC
						return;
					}

					int r = sd_cmdx(spi, 17, addr);
					if (r & 0x7E)
					{
						work_flags |= SD_FLAG_ERROR;
						if (w->receiver)
							w->receiver->sd_read_complete(this, w->sector, w->buf, w->status + 1);
						// TODO: handle error
						return;
					}

					w->status = SD_READ_STATUS_WAIT_TRAN;
					printf("WAIT TRAN: ");
					// deliberate fall-through
				}
				case SD_READ_STATUS_WAIT_TRAN:
				{
					uint8_t r;
					r = spi->transfer(0xFF);
					printf("0x%X ", r);
					if (r != 0xFE)
					{
						if (r == 0xFF)
						{
							// TODO: schedule work_queue_work maybe 25uS in the future
// 							request_work(25);
							work_flags |= SD_FLAG_REQ_WORK;
						}
						else
						{
							work_flags &= ~SD_FLAG_REQ_WORK;
							work_flags |= SD_FLAG_ERROR;
							// error!
							if (w->receiver)
								w->receiver->sd_read_complete(this, w->sector, w->buf, w->status + 1);
						}
						return;
					}
					work_flags &= ~SD_FLAG_REQ_WORK;
					w->status = SD_READ_STATUS_DMA;
					// deliberate fall-through
				}
				case SD_READ_STATUS_DMA:
				{
					work_flags |= SD_FLAG_RUNNING;

					dma_txmem.setup(&txm, 4);
					dma_txmem.auto_increment = DMA_NO_INCREMENT;

					dma_rxmem.setup(w->buf, 512);

					dma_tx.setup(512);
					dma_rx.setup(512);

					dma_rx.begin();
					dma_tx.begin();

					w->status = SD_READ_STATUS_CHECKSUM;

					break;
				}
				case SD_READ_STATUS_CHECKSUM:
					spi->transfer(0xFF);
					spi->transfer(0xFF);

					work_flags &= ~SD_FLAG_RUNNING;

					switch(work_stack->action)
					{
						case SD_WORK_ACTION_READ:
							if (work_stack->receiver)
								work_stack->receiver->sd_read_complete(this, work_stack->sector, work_stack->buf, 0);
							break;
						case SD_WORK_ACTION_WRITE:
							if (work_stack->receiver)
								work_stack->receiver->sd_write_complete(this, work_stack->sector, work_stack->buf, 0);
							break;
						default:
							break;
					}

					work_stack_pop();

					break;
			}
		};
		break;
		default:
			break;
	}
}

void SD::work_stack_pop()
{
	if (work_stack == NULL)
	{
		work_flags &= ~(SD_FLAG_RUNNING | SD_FLAG_REQ_WORK);
		return;
	}

	// remove item from stack
	sd_work_stack_t* w = work_stack;
	work_stack = w->next;

	// move work stack item to garbage collector stack
	w->next = gc_stack;
	gc_stack = w;

	// next item
	if (work_stack)
		work_stack_work();
	else
		work_flags &= ~(SD_FLAG_RUNNING | SD_FLAG_REQ_WORK);

	work_stack_debug();
}

void SD::work_stack_debug()
{
	sd_work_stack_t* w = work_stack;

	printf("Work Stack:\n");
	while (w)
	{
		printf("\tItem %p:\n", w);
		printf("\t\tAction: %d\n\t\tsector: %lu\n\t\tbuffer: %p\n\t\tStatus: %d\n\t\tReceiver: %p\n\t\tNext: %p\n", w->action, w->sector, w->buf, w->status, w->receiver, w->next);
		w = w->next;
	};
	printf("End Stack\n");
}

void SD::dma_begin(DMA* dma, dma_direction_t direction)
{
	spi->dma_begin(dma, direction);
}

void SD::dma_complete(DMA* dma, dma_direction_t direction)
{
	spi->dma_complete(dma, direction);

	if (direction == DMA_RECEIVER)
	{

		// TODO: do something interesting with transferred block
		work_stack_work();
	}
}

void SD::dma_configure(dma_config* config)
{
	spi->dma_configure(config);
}

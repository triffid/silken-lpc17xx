#include "SD.h"

#include <cstddef>
#include <cstdlib>
#include <cstdio>

#include "platform_utils.h"

#include "mri.h"

#define TRACE(...) printf(__VA_ARGS__)

#define CMD_TIMEOUT 32
#define READ_TIMEOUT 512
#define WRITE_TIMEOUT 512

typedef enum {
    SD_CMD_GO_IDLE_STATE =  0,
    SD_CMD_SEND_IF_COND  =  8,
    SD_CMD_SEND_CSD      =  9,
    SD_CMD_SEND_CID      = 10,
    SD_CMD_STOP_TRAN     = 12,
    SD_CMD_READ_BLOCK    = 17,
    SD_CMD_READ_BLOCKS   = 18,
    SD_CMD_WRITE_BLOCK   = 24,
    SD_CMD_WRITE_BLOCKS  = 25,
    SD_CMD_APP_CMD       = 55,
    SD_CMD_READ_OCR      = 58
} SD_CMD_NUM;

typedef enum {
    SD_ACMD_SET_WR_ERASE_BLOCKS = 23,
    SD_ACMD_SEND_OP_COND        = 41
} SD_ACMD_NUM;

typedef enum {
	SD_READ_STATUS_START,
	SD_READ_STATUS_WAIT_TRAN,
    SD_READ_STATUS_CONTINUE_MULTI,
    SD_READ_STATUS_DMA,
	SD_READ_STATUS_CHECKSUM,
    SD_READ_STATUS_BUFFER_DIRTY
} SD_READ_STATUS;

typedef enum {
	SD_WRITE_STATUS_START,
    SD_WRITE_STATUS_WAIT_BSY,
    SD_WRITE_STATUS_DMA,
    SD_WRITE_STATUS_CHECKSUM,
    SD_WRITE_STATUS_WAIT_RESPONSE,
    SD_WRITE_STATUS_BUFFER_DIRTY
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

// #undef printf
// #define printf(...) do {} while (0)

static uint32_t ext_bits(uint8_t *data, uint32_t msb, uint32_t lsb) {
	uint32_t bits = 0;
	uint32_t size = 1 + msb - lsb;
	for(uint32_t i=0; i<size; i++) {
		uint32_t position = lsb + i;
		uint32_t byte = 15 - (position >> 3);
		uint32_t bit = position & 0x7;
		uint32_t value = (data[byte] >> bit) & 1;
		bits |= value << i;
	}
	return bits;
}

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

// 	printf("spi_cmdx Send: ");
// 	for (uint32_t q = 0; q < sizeof(spi_cmd); q++)
// 		printf("0x%X ", spi_cmd.packet[q]);
// 	printf("\n");
	
	spi->send_block(spi_cmd.packet, sizeof(spi_cmd));

// 	printf("         Recv: ");
	
	int i;
	uint8_t r;
	for (i = 0; i < CMD_TIMEOUT; i++)
	{
		if (((r = spi->transfer(0xFF)) & 0x80) == 0)
			break;
// 		printf("0x%X ", r);
	}
// 	printf("0x%X\n", r);

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
		
// 		printf("<0x%x> ", b[0]);
		if (--i <= 0)
			// CMD timeout
			return -1;
	}
	while (b[0] == 0xFF);
	
	spi->recv_block(b + 1, size - 1, 0xFF);

// 	printf("    RecvBlock: ");
// 	for (int q = 0; q < size; q++)
// 		printf("0x%X%c", b[q], ((q & 31) == 31)?'\n':' ');
// 	printf("\n");
	
	return size;
}

static int sd_data(SPI* spi, void* buf, int size)
{
	uint8_t* b = (uint8_t*) buf;

	int i = CMD_TIMEOUT;

	do {
		b[0] = spi->transfer(0xFF);

// 		printf("<0x%x> ", b[0]);
		if (--i <= 0)
			// CMD timeout
			return -1;
	}
	while (b[0] != 0xFE);

	spi->recv_block(b, size, 0xFF);

// 	printf("    RecvBlock: ");
// 	for (int q = 0; q < size; q++)
// 		printf("0x%X%c", b[q], ((q & 31) == 31)?'\n':' ');
// 	printf("\n");

	return size;
}

static int sd_cmd(SPI* spi, int cmd, uint32_t arg)
{
	int r = sd_cmdx(spi, cmd, arg);
	spi->end_transaction();
	
	return r;
}

static inline int sd_cmd_go_idle_state(SPI* spi)
{
	return sd_cmd(spi, SD_CMD_GO_IDLE_STATE, 0);
}

static inline int sd_cmd_send_if_cond(SPI* spi)
{
	union {
		uint8_t b[4];
		uint32_t w;
	} buf;
	
	buf.w = 0;
	
	int r;
	
	if ((r = sd_cmdx(spi, SD_CMD_SEND_IF_COND, 0x1AA)) & 0x7A)
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

static inline int sd_cmd_read_ocr(SPI* spi, uint32_t* ocr)
{
	union {
		uint8_t b[4];
		uint32_t ocr;
	} buf;
	
	int r;
	
	if ((r = sd_cmdx(spi, SD_CMD_READ_OCR, 0)) & 0x7E)
		return -1;
	
	if (sd_response(spi, buf.b, 4) < 0)
		return -1;
	
	spi->end_transaction();
	
	*ocr = ntohl(buf.ocr);
	
	return r;
}

static inline int sd_acmd_send_op_cond(SPI* spi)
{
	int r;
	
	int i = READ_TIMEOUT;
	
	do
	{
		if ((r = sd_cmd(spi, SD_CMD_APP_CMD, 0)) & 0x7E)
			return -1;
		
		if ((r = sd_cmd(spi, SD_ACMD_SEND_OP_COND, SD_HIGH_CAPACITY)) & 0x7E)
			return -1;
		
		if (--i <= 0)
			return -1;
	}
	while (r == 1);
	
	return r;
}

static inline int sd_acmd_set_wr_erase_blocks(SPI* spi, uint32_t n_blocks)
{
    int r;

    if ((r = sd_cmd(spi, SD_CMD_APP_CMD, 0)) & 0x7E)
        return -1;

    if ((r = sd_cmd(spi, SD_ACMD_SET_WR_ERASE_BLOCKS, n_blocks)) & 0x7E)
        return -1;

    return r;
}

static int sd_cmd_send_csd(SPI* spi, void* CSD)
{
	int r;

	r = sd_cmdx(spi, SD_CMD_SEND_CSD, 0);
	if (r & 0x7E)
	{
		spi->end_transaction();
		return -1;
	}

	if (sd_data(spi, CSD, 16) < 0)
	{
		spi->end_transaction();
		return -1;
	}

	spi->end_transaction();

	return r;
}

static int sd_cmd_send_cid(SPI* spi, void* CID)
{
	int r;

	r = sd_cmdx(spi, SD_CMD_SEND_CID, 0);
	if (r & 0x7E)
	{
		spi->end_transaction();
		return -1;
	}

	if (sd_data(spi, CID, 16) < 0)
	{
		spi->end_transaction();
		return -1;
	}

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
	gc_stack   = NULL;

	work_flags = SD_FLAG_IDLE;
}

int SD::init()
{
	int i, r;
	
	card_type = SD_TYPE_NONE;
	
	spi->set_frequency(25000);
	
	for (i = 0; i < CMD_TIMEOUT; i++)
	{
		r = sd_cmd_go_idle_state(spi);
		if (r == 1)
			break;
	}
	
	if (i >= CMD_TIMEOUT)
		return -1;
	
	r = sd_cmd_send_if_cond(spi);
	
	if (r & 4)
		card_type = SD_TYPE_MMC;
	
	// TODO: support MMC
	if (r != 1)
		return -2;
	
	r = sd_acmd_send_op_cond(spi);
	if (r != 0)
		return -3;
	
	uint32_t ocr = 0;
	
	do
	{
		r = sd_cmd_read_ocr(spi, &ocr);

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

	uint8_t csd[16];
	
	r = sd_cmd_send_csd(spi, csd);
	if (r & 0x7E)
		return -6;

	if (ext_bits(csd, 127, 126) == 0)
	{
		sector_count = (ext_bits(csd, 75, 62) + 1)
		       * (1 << (ext_bits(csd, 49, 47) + 2))
		       * (1 << (ext_bits(csd, 83, 80) - 9));
	}
	else if (ext_bits(csd, 127, 126) == 1)
	{
		sector_count = (ext_bits(csd, 69, 48) + 1) * 1024;
	}
	else
		return -7;
	
	printf("\nTotal Sectors: %lu\n", sector_count);
	printf("Card Size: %lu.%lu%c\n", (sector_count >= 2097152)?(sector_count / 2097152):(sector_count / 2048), (sector_count >= 2097152)?((sector_count / 209715) % 10):((sector_count / 205) % 10), (sector_count >= 2097152)?('G'):('M') );

	uint8_t cid[16];

	r = sd_cmd_send_cid(spi, cid);
	if (r & 0x7E)
		return -8;

	printf("MID %lu (%c%c) %c%c%c%c%c s/n:%lu date:%lu/%lu\n",
		ext_bits(cid, 127, 120),
		   (int) ext_bits(cid, 119, 112), (int) ext_bits(cid, 111, 104),
		   (int) ext_bits(cid, 103,  96), (int) ext_bits(cid,  95,  88), (int) ext_bits(cid,  87,  80), (int) ext_bits(cid,  79,  72), (int) ext_bits(cid,  71,  64),
		ext_bits(cid,  55,  24),
		ext_bits(cid,  11,   8), ext_bits(cid,  19,  12) + 2000
	);
	
	return 1;
}

void SD::on_idle()
{
	if ((work_flags & (SD_FLAG_RUNNING | SD_FLAG_REQ_WORK)) == (SD_FLAG_RUNNING | SD_FLAG_REQ_WORK))
		work_stack_work();

	while (gc_stack)
	{
		sd_work_stack_t* w = gc_stack;
		gc_stack = w->next;
		free(w);
	}
}

SD_CARD_TYPE SD::get_type()
{
	return card_type;
}

uint32_t SD::n_sectors()
{
	return sector_count;
}

int SD::begin_read(uint32_t sector, uint32_t n_sectors, void* buf, SD_async_receiver* receiver)
{
	sd_work_stack_t* w;
	if (gc_stack)
	{
		// reuse existing item from gc stack to reduce calls to malloc/free
		w = gc_stack;
		gc_stack = w->next;
	}
	else
		w = (sd_work_stack_t*) malloc(sizeof(sd_work_stack_t));

	w->action     = SD_WORK_ACTION_READ;
	w->buf        = buf;
	w->sector     = sector;
    if (n_sectors > 1)
        w->end_sector = sector + n_sectors - 1;
    else
        w->end_sector = 0;
	w->receiver   = receiver;
	w->status     = SD_READ_STATUS_START;
	w->next       = NULL;

    __disable_irq();

	if (work_stack)
	{
        sd_work_stack_t* x = work_stack;

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

	return 0;
}

void SD::work_stack_work(void)
{
	switch(work_stack->action)
	{
		case SD_WORK_ACTION_READ:
            work_stack_read();
            break;
        case SD_WORK_ACTION_WRITE:
            work_stack_write();
            break;
		default:
			break;
	}
}

void SD::work_stack_read()
{
    sd_work_stack_t* w = work_stack;

//     printf("SD:{R %d/%p LBA 0x%02lX}\n", w->status, w, w->sector);

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

            int r = sd_cmdx(spi, w->end_sector?SD_CMD_READ_BLOCKS:SD_CMD_READ_BLOCK, addr);
            if (r & 0x7E)
            {
                work_flags |= SD_FLAG_ERROR;
                if (w->receiver)
                    w->receiver->sd_read_complete(this, w->sector, w->buf, w->status + 1);
                // TODO: handle error
                return;
            }

            w->status = SD_READ_STATUS_WAIT_TRAN;
            //                  printf("WAIT TRAN: ");
            // deliberate fall-through
        }
        case SD_READ_STATUS_WAIT_TRAN:
        case SD_READ_STATUS_CONTINUE_MULTI:
        {
            uint8_t r;
            r = spi->transfer(0xFF);

            // I have no idea why, but removing this printf causes us to hard-lock
//             printf("0x%X", r);

            if (r != 0xFE)
            {
                if (r == 0xFF)
                {
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

//             TRACE("DMA:setup\n");
            dma_tx.setup(512);
            dma_rx.setup(512);
            TRACE("DMA:setup complete\n");

            w->status = SD_READ_STATUS_CHECKSUM;

            for (volatile uint32_t r = 1UL<<24; r; r--);

            dma_rx.begin();
            dma_tx.begin();

            break;
        }
        case SD_READ_STATUS_CHECKSUM:
            spi->transfer(0xFF);
            spi->transfer(0xFF);

            sd_work_stack_t* w = work_stack;

            TRACE("DMA:CSUM\n");
            for (volatile uint32_t r = 1UL<<19; r; r--);

            // we must pop first, in case the read_complete event requests another read.
            // in that case, it's advantageous to have the work item in the GC queue already so it can be reused
            if (w->sector >= w->end_sector)
            {
                work_stack_pop();
                if (w->end_sector)
                    sd_cmd(spi, 12, 0);
            }
            else
                w->status = SD_READ_STATUS_BUFFER_DIRTY;

            if (w->receiver)
                w->receiver->sd_read_complete(this, w->sector, w->buf, 0);

            break;
    }
}

void SD::work_stack_write()
{
    sd_work_stack_t* w = work_stack;

    switch(w->status)
    {
        // TODO
        case SD_WRITE_STATUS_START:
            if (w->end_sector)
            {
                sd_cmd(spi, SD_CMD_APP_CMD, 0);
                sd_cmd(spi, SD_ACMD_SET_WR_ERASE_BLOCKS, w->end_sector - w->sector + 1);

                uint32_t addr;

                if (card_type == SD_TYPE_SDHC)
                    addr = w->sector;
                else if (card_type == SD_TYPE_SD)
                    addr = w->sector << 9;
                else
                {
                    work_flags |= SD_FLAG_ERROR;
                    if (w->receiver)
                        w->receiver->sd_write_complete(this, w->sector, w->buf, w->status + 1);
                    // TODO: support MMC
                    return;
                }

                int r = sd_cmd(spi, w->end_sector?SD_CMD_WRITE_BLOCKS:SD_CMD_WRITE_BLOCK, addr);
                if (r & 0x7E)
                {
                    work_flags |= SD_FLAG_ERROR;
                    if (w->receiver)
                        w->receiver->sd_write_complete(this, w->sector, w->buf, w->status + 1);
                    return;
                }

                w->status = SD_WRITE_STATUS_DMA;
                // deliberate fall-through
            }
        case SD_WRITE_STATUS_DMA:
        {
            work_flags |= SD_FLAG_RUNNING;

            dma_txmem.setup(w->buf, 512);
            dma_txmem.auto_increment = DMA_AUTO_INCREMENT;

            dma_rxmem.setup(&txm, 4);
            dma_rxmem.auto_increment = DMA_NO_INCREMENT;

            dma_tx.setup(512);
            dma_rx.setup(512);

            dma_rx.begin();
            dma_tx.begin();

            w->status = SD_WRITE_STATUS_CHECKSUM;

            break;
        };
            break;
        case SD_WRITE_STATUS_CHECKSUM:
            spi->transfer(0xFF);
            spi->transfer(0xFF);

            w->status = SD_WRITE_STATUS_WAIT_RESPONSE;

            // deliberate fall-through
        case SD_WRITE_STATUS_WAIT_RESPONSE:
        {
            uint8_t r = spi->transfer(0xFF);
            if (r != 0xFF)
            {
                work_flags &= ~SD_FLAG_REQ_WORK;

                if ((r & 31) == 0x5)
                {
                    // data accepted
                    w->status = SD_WRITE_STATUS_BUFFER_DIRTY;

                    if (w->receiver)
                        w->receiver->sd_write_complete(this, w->sector, w->buf, 0);

                    if (w->status != SD_WRITE_STATUS_WAIT_BSY)
                        break;
                }
                else
                {
                    sd_cmd(spi, SD_CMD_STOP_TRAN, 0);
                    if (w->receiver)
                        w->receiver->sd_write_complete(this, w->sector, w->buf, r);
                }
            }
            else
            {
                work_flags |= SD_FLAG_REQ_WORK;
                break;
            }
        };

            // deliberate fall-through

        case SD_WRITE_STATUS_WAIT_BSY:
        {
            uint8_t r = spi->transfer(0xFF);
            if (r != 0x00)
            {

            }
            else
                work_flags |= SD_FLAG_REQ_WORK;
        };
        case SD_WRITE_STATUS_BUFFER_DIRTY:
            break;
        default:
            break;
    }
}

void SD::clean_buffer(void* buf)
{
    switch(work_stack->action)
    {
        case SD_WORK_ACTION_READ:
            if (work_stack->status == SD_READ_STATUS_BUFFER_DIRTY)
            {
                work_stack->status = SD_READ_STATUS_CONTINUE_MULTI;
                work_stack->buf = buf;
                work_stack->sector++;
//                 printf("WAIT TRAN: ");
                work_stack_work();
            }
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
		work_flags |= SD_FLAG_REQ_WORK;
	else
		work_flags &= ~(SD_FLAG_RUNNING | SD_FLAG_REQ_WORK);

// 	work_stack_debug();
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
    TRACE("SD: dma complete %p\n", dma);

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

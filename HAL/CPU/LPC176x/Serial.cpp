#include "Serial.h"

#include <cstdint>
#include <cstring>

#include "platform_memory.h"

#include "mri.h"

#include "lpc17xx_pinsel.h"
#include "lpc17xx_clkpwr.h"
#include "lpc17xx_uart.h"

// PCLKSEL0
#define PCLK_UART0 6
#define PCLK_UART1 8
// PCLKSEL1
#define PCLK_UART2 16
#define PCLK_UART3 18

#define BUFSIZE 128

struct _platform_serialdata
{
	uint8_t* txbuf;
	uint16_t txhead;
	uint16_t txtail;

	uint8_t* rxbuf;
	uint16_t rxhead;
	uint16_t rxtail;
	
	LPC_UART_TypeDef* u;
};

static Serial* instance[4];

extern "C" {
// 	static struct _platform_serialdata serialdatas[2];
// 	static int serialdata_index = 0;
	
	void UART0_IRQHandler(void)
	{
		if (instance[0]) instance[0]->isr();
	}
	void UART1_IRQHandler(void)
	{
		if (instance[1]) instance[1]->isr();
	}
	void UART2_IRQHandler(void)
	{
		if (instance[2]) instance[2]->isr();
	}
	void UART3_IRQHandler(void)
	{
		if (instance[3]) instance[3]->isr();
	}
}

static LPC_UART_TypeDef* NXPUART_init(PinName txpin, PinName rxpin, int baudrate, void* set_instance)
{
	int port;
	LPC_UART_TypeDef* u;

	PINSEL_CFG_Type PinCfg;

	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;

	if (txpin == P0_2 && rxpin == P0_3) {
		port = 0;
		u = LPC_UART0;
		PinCfg.Funcnum = 1;
	}
	else if (txpin == P0_0 && rxpin == P0_1) {
		port = 3;
		u = LPC_UART3;
		PinCfg.Funcnum = 2;
	}
	else if (txpin == P0_10 && rxpin == P0_11) {
		port = 2;
		u = LPC_UART2;
		PinCfg.Funcnum = 1;
	}
	else if (txpin == P0_15 && rxpin == P0_16) {
		port = 1;
		u = (LPC_UART_TypeDef *) LPC_UART1;
		PinCfg.Funcnum = 1;
	}
	else if (txpin == P0_25 && rxpin == P0_26) {
		port = 3;
		u = LPC_UART3;
		PinCfg.Funcnum = 3;
	}
	else if (txpin == P2_0 && rxpin == P2_1) {
		port = 1;
		u = (LPC_UART_TypeDef *) LPC_UART1;
		PinCfg.Funcnum = 2;
	}
	else if (txpin == P2_8 && rxpin == P2_9) {
		port = 2;
		u = LPC_UART2;
		PinCfg.Funcnum = 2;
	}
	else if (txpin == P4_28 && rxpin == P4_29) {
		port = 3;
		u = LPC_UART3;
		PinCfg.Funcnum = 3;
	}
	else {
		//TODO: software serial
		port = -1;
		return NULL;
	}

	PinCfg.Portnum = (txpin >> 5) & 7;
	PinCfg.Pinnum = (txpin & 0x1F);
	PINSEL_ConfigPin(&PinCfg);

	PinCfg.Portnum = (rxpin >> 5) & 7;
	PinCfg.Pinnum = (rxpin & 0x1F);
	PINSEL_ConfigPin(&PinCfg);

	uint32_t PCLK = SystemCoreClock;
	// First we check to see if the basic divide with no DivAddVal/MulVal
	// ratio gives us an integer result. If it does, we set DivAddVal = 0,
	// MulVal = 1. Otherwise, we search the valid ratio value range to find
	// the closest match. This could be more elegant, using search methods
	// and/or lookup tables, but the brute force method is not that much
	// slower, and is more maintainable.
	uint16_t DL = PCLK / (16 * baudrate);

	uint8_t DivAddVal = 0;
	uint8_t MulVal = 1;
	int hit = 0;
	uint16_t dlv;
	uint8_t mv, dav;
	if ((PCLK % (16 * baudrate)) != 0) { // Checking for zero remainder
		int err_best = baudrate, b;
		for (mv = 1; mv < 16 && !hit; mv++)
		{
			for (dav = 0; dav < mv; dav++)
			{
				// baudrate = PCLK / (16 * dlv * (1 + (DivAdd / Mul))
				// solving for dlv, we get dlv = mul * PCLK / (16 * baudrate * (divadd + mul))
				// mul has 4 bits, PCLK has 27 so we have 1 bit headroom which can be used for rounding
				// for many values of mul and PCLK we have 2 or more bits of headroom which can be used to improve precision
				// note: X / 32 doesn't round correctly. Instead, we use ((X / 16) + 1) / 2 for correct rounding

				if ((mv * PCLK * 2) & 0x80000000) // 1 bit headroom
					dlv = ((((2 * mv * PCLK) / (baudrate * (dav + mv))) / 16) + 1) / 2;
				else // 2 bits headroom, use more precision
					dlv = ((((4 * mv * PCLK) / (baudrate * (dav + mv))) / 32) + 1) / 2;

				// datasheet says if DLL==DLM==0, then 1 is used instead since divide by zero is ungood
				if (dlv == 0)
					dlv = 1;

				// datasheet says if dav > 0 then DL must be >= 2
				if ((dav > 0) && (dlv < 2))
					dlv = 2;

				// integer rearrangement of the baudrate equation (with rounding)
				b = ((PCLK * mv / (dlv * (dav + mv) * 8)) + 1) / 2;

				// check to see how we went
				b = abs(b - baudrate);
				if (b < err_best)
				{
					err_best = b;

					DL = dlv;
					MulVal = mv;
					DivAddVal = dav;

					if (b == baudrate)
					{
						hit = 1;
						break;
					}
				}
			}
		}
	}

	IRQn c;

	switch(port)
	{
		case 0:
			LPC_SC->PCONP |= CLKPWR_PCONP_PCUART0;
			LPC_SC->PCLKSEL0 = (LPC_SC->PCLKSEL0 & ~(3 << PCLK_UART0)) | 1 << PCLK_UART0;
			c = UART0_IRQn;
			break;
		case 1:
			LPC_SC->PCONP |= CLKPWR_PCONP_PCUART1;
			LPC_SC->PCLKSEL0 = (LPC_SC->PCLKSEL0 & ~(3 << PCLK_UART1)) | 1 << PCLK_UART1;
			c = UART1_IRQn;
			break;
		case 2:
			LPC_SC->PCONP |= CLKPWR_PCONP_PCUART2;
			LPC_SC->PCLKSEL1 = (LPC_SC->PCLKSEL1 & ~(3 << PCLK_UART2)) | 1 << PCLK_UART2;
			c = UART2_IRQn;
			break;
		case 3:
			LPC_SC->PCONP |= CLKPWR_PCONP_PCUART3;
			LPC_SC->PCLKSEL1 = (LPC_SC->PCLKSEL1 & ~(3 << PCLK_UART3)) | 1 << PCLK_UART3;
			c = UART3_IRQn;
			break;
	}
	
	if (set_instance)
		instance[port] = (Serial*) set_instance;

	u->LCR |= UART_LCR_DLAB_EN;
	u->DLM = (DL >> 8) & 0xFF;
	u->DLL = (DL & 0xFF);
	u->LCR &= ~(UART_LCR_DLAB_EN) & UART_LCR_BITMASK;
	u->FDR = (DivAddVal & 0xF) | ((MulVal & 0xF) << 4);

	u->FCR = (UART_FCR_FIFO_EN | UART_FCR_RX_RS | UART_FCR_TX_RS | UART_FCR_TRG_LEV2);
	u->LCR = UART_LCR_WLEN8;
	u->ICR = 0;
	u->TER |= UART_TER_TXEN;
	
	if (c < 128)
		NVIC_EnableIRQ(c);

	return u;
}

extern "C" void* _sbrk(int);
Serial::Serial(PinName tx, PinName rx, int baud)
{
	// TODO: stop futzing around, do this properly
// 	data = (_platform_serialdata*) AHB0.alloc(sizeof(_platform_serialdata));
// 	__debugbreak();
// 	data = &serialdatas[serialdata_index++];
	data = (_platform_serialdata*) malloc(sizeof(_platform_serialdata));

	uint8_t* block = (uint8_t*) AHB0.alloc(BUFSIZE * 2);
	data->txbuf = block;
	data->rxbuf = block + BUFSIZE;
	
	data->txhead = data->txtail = 0;
	data->rxhead = data->rxtail = 0;

	data->u = NXPUART_init(tx, rx, baud, this);
	
	data->u->IER = UART_IER_RBRINT_EN | UART_IER_RLSINT_EN;
}

Serial::~Serial()
{
	switch((uintptr_t) data->u)
	{
		case ((uintptr_t) LPC_UART0):
			NVIC_DisableIRQ(UART0_IRQn);
			break;
		case ((uintptr_t) LPC_UART1):
			NVIC_DisableIRQ(UART1_IRQn);
			break;
		case ((uintptr_t) LPC_UART2):
			NVIC_DisableIRQ(UART2_IRQn);
			break;
		case ((uintptr_t) LPC_UART3):
			NVIC_DisableIRQ(UART3_IRQn);
			break;
	}
	AHB0.dealloc(data->txbuf);
	free(data);
}

Serial* Serial::set_mode(MODE)
{
    return this;
}

int Serial::can_write(void)
{
	return (data->txtail + BUFSIZE - data->txhead - 1) & (BUFSIZE - 1);
}

int Serial::write(const void* buf, int buflen)
{
	int t = 0;

	int cw = can_write();

	if (buflen > cw)
		buflen = cw;

	if ((data->txhead + buflen) & ~(BUFSIZE - 1))
	{
		t = BUFSIZE - data->txhead;
		memcpy(&data->txbuf[data->txhead], buf, t);
		buf = (void*) (((uintptr_t) buf) + t);
		buflen -= t;
		data->txhead = 0;
	}
	
	memcpy(&data->txbuf[data->txhead], buf, buflen);
	t += buflen;
	data->txhead = (data->txhead + buflen) & (BUFSIZE - 1);
	
	if ((data->u->IER & UART_IER_THREINT_EN) == 0)
		tx_isr();
	
	return t;
}

int Serial::can_read(void)
{
	return (data->rxhead + BUFSIZE - data->rxtail) & (BUFSIZE - 1);
}

int Serial::read(void* buf, int buflen)
{
	int cr = can_read();
	int t = 0;

	if (buflen > cr)
		buflen = cr;

	if ((data->rxtail + buflen) & ~(BUFSIZE - 1))
	{
		t = BUFSIZE - data->rxtail;
		memcpy(buf, &data->rxbuf[data->rxtail], t);
		buf = (void*) (((uintptr_t) buf) + t);
		buflen -= t;
		data->rxtail = 0;
	}

	memcpy(buf, &data->rxbuf[data->rxtail], buflen);
	t += buflen;
	data->rxtail += buflen;

	return t;
}

void Serial::isr()
{
	switch(data->u->IIR & UART_IIR_INTID_MASK)
	{
		case UART_IIR_INTID_RLS: // receive line status (line status change, errors)
			break;
		case UART_IIR_INTID_RDA: // receive data available (receive fifo is >= trigger level)
			// deliberate fall-through
		case UART_IIR_INTID_CTI: // character timeout (receive fifo not empty, but not >= trigger level for 3-5 character times)
			rx_isr();
			break;
		case UART_IIR_INTID_THRE:
			tx_isr();
			break;
	}
}

void Serial::tx_isr()
{
	if (data->txtail != data->txhead)
	{
		for (int i = 0; i < 16 && data->txtail != data->txhead; i++)
		{
			data->u->THR = data->txbuf[data->txtail];
			data->txtail = (data->txtail + 1) & (BUFSIZE - 1);
		}
		data->u->IER |= UART_IER_THREINT_EN;
	}
	else
	{
		data->u->IER &= ~(UART_IER_THREINT_EN);
	}
}

void Serial::rx_isr()
{
	while (data->u->LSR & UART_LSR_RDR)
	{
		char c = data->u->RBR;
		data->rxbuf[data->rxhead] = c;
		data->rxhead = (data->rxhead + 1) & (BUFSIZE - 1);
		if (data->rxhead == data->rxtail)
			data->rxtail = (data->rxtail + 1) & (BUFSIZE - 1);
	}
}

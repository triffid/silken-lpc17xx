#ifndef _SERIAL_H
#define _SERIAL_H

#include "pins.h"

struct _platform_serialdata;

class Serial
{
public:
	Serial(PinName tx, PinName rx, int baud);
	~Serial();

	typedef enum
	{
		MODE_BLOCKING,
		MODE_NONBLOCK
	} MODE;

	Serial* set_mode(MODE);

	int can_write(void);
	int write(const void* buf, int buflen);

	int can_read(void);
	int read(void* buf, int buflen);

	void isr(void);
	void tx_isr(void);
	void rx_isr(void);

private:
	struct _platform_serialdata* data;
};

#endif /* _SERIAL_H */

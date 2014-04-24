#ifndef _GPIO_H
#define	_GPIO_H

#include <cstdint>

#define	GPIO_DIR_INPUT 0
#define	GPIO_DIR_OUTPUT 1

#include "pins.h"

class GPIO;
class GPIO
{
public:
	GPIO(PinName);

	GPIO* setup(void);
	GPIO* set_direction(uint8_t direction);
	GPIO* output(void);
	GPIO* input(void);
	GPIO* write(uint8_t value);
	GPIO* set(void);
	GPIO* clear(void);
	
	uint8_t get(void);

	PinName pin;
};

#endif /* _GPIO_H */

#include "clock.h"

#include <cstdlib>

#include "platform_utils.h"

#include "lpc17xx_systick.h"

#include "gpio.h"
extern GPIO* isp_btn;

struct _clock_data
{
    volatile uint64_t ticks;

    uint32_t flag_issuer;

    volatile uint32_t flag_1s;
    volatile uint32_t flag_10ms;

    uint8_t countdown_1s;
};

Clock* Clock::instance = (Clock*) NULL;

extern "C"
{
    static uint64_t get_ticks(clock_data* d)
    {
        __disable_irq();
        uint64_t r = d->ticks;
        __enable_irq();
        return r;
    }

    void SysTick_Handler()
    {
        if (Clock::instance)
            Clock::instance->irq();
    }
}

Clock::Clock()
{
    data = (clock_data*) malloc(sizeof(clock_data));
    data->ticks = 0;
    data->flag_issuer = 1;
    data->countdown_1s = 100;
    data->flag_1s = 0;
    data->flag_10ms = 0;

    instance = this;

    // set up LPC17xx SysTick
    SYSTICK_InternalInit(granularity_ms());
    SYSTICK_Cmd(ENABLE);
    SYSTICK_IntCmd(ENABLE);
}

uint64_t Clock::time_s(void)
{
    return get_ticks(data) / 100;
}

uint64_t Clock::time_ms(void)
{
    return get_ticks(data) * 10;
}

uint32_t Clock::granularity_ms(void)
{
    return 10;
}

uint32_t Clock::request_flag(void)
{
    uint32_t r = data->flag_issuer;
    data->flag_issuer <<= 1;
    return r;
}

bool Clock::flag_1s_test(uint32_t flag)
{
    bool r = false;
    __disable_irq();
    if (data->flag_1s & flag)
    {
        r = true;
        data->flag_1s &= ~flag;
    }
    __enable_irq();
    return r;
}

bool Clock::flag_10ms_test(uint32_t flag)
{
    bool r = false;
    __disable_irq();
    if (data->flag_10ms & flag)
    {
        r = true;
        data->flag_10ms &= ~flag;
    }
    __enable_irq();
    return r;
}

void Clock::irq()
{
    data->ticks++;

    data->flag_10ms = ~0UL;

    if (--data->countdown_1s == 0)
    {
        data->countdown_1s = 100;
        data->flag_1s = ~0UL;
    }

    if (isp_btn->get() == 0)
        __debugbreak();
}

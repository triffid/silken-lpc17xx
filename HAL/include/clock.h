#ifndef _CLOCK_H
#define _CLOCK_H

#include <cstdint>

struct _clock_data;
typedef struct _clock_data clock_data;

class Clock
{
public:
    Clock();

    uint64_t time_s(void);
    uint64_t time_ms(void);

    uint32_t granularity_ms(void); // informational

    uint32_t request_flag(void);

    bool flag_1s_test(uint32_t flag);
    bool flag_10ms_test(uint32_t flag);

    static Clock* instance; // singleton pointer
    void irq(void);         // irq receiver
private:
    clock_data* data;
};

#endif /* _CLOCK_H */

#include "platform_memory.h"

extern uint8_t __AHB0_dyn_start;
extern uint8_t __AHB0_dyn_end;

extern uint8_t __AHB1_dyn_start;
extern uint8_t __AHB1_dyn_end;

MemoryPool AHB0(&__AHB0_dyn_start, &__AHB0_dyn_end - &__AHB0_dyn_start);
MemoryPool AHB1(&__AHB1_dyn_start, &__AHB1_dyn_end - &__AHB1_dyn_start);

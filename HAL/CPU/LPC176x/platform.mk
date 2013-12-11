MCU      = cortex-m3

CDEFS   += __LPC17XX__ ARM_MATH_CM3

FLAGS   += -march=armv7-m -mtune=$(MCU)
FLAGS   += -mthumb -mthumb-interwork

LDFLAGS += -Wl,-e,Reset_Handler,-T,HAL/CPU/$(CPU)/$(CPU).ld

all: size

size: $(O)/$(PROJECT).elf
	@echo
	@echo $$'           \033[1;4m  SIZE        LPC1769\033[0m'
	@$(OBJDUMP) -h $^ | perl -MPOSIX -ne '/.(text|rodata)\s+([0-9a-f]+)/ && do { $$a += eval "0x$$2" }; END { printf "  FLASH    %6d bytes  %2d%% of %3dkb\n", $$a, ceil($$a * 100 / ((512 - 16) * 1024)), 512 - 16 }'
	@$(OBJDUMP) -h $^ | perl -MPOSIX -ne '/.(data|bss)\s+([0-9a-f]+)/    && do { $$a += eval "0x$$2" }; END { printf "  RAM      %6d bytes  %2d%% of %3dkb\n", $$a, ceil($$a * 100 / ( 16 * 1024)),  16 }'

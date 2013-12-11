#
#
#

APPBAUD  = 1000000

CPU      = LPC176x
BOARD    = Smoothieboard

O        = build

CONSOLE  = /dev/arduino

################################################################################
#                                                                              #
# Shouldn't need to touch anything below here                                  #
#                                                                              #
################################################################################

PROJECT   = Silken_$(CPU)_$(BOARD)

include HAL/CPU/$(CPU)/platform.mk

CSRC      = $(shell find src/ -name '*.c')
CXXSRC    = $(shell find src/ -name '*.cpp')
ASRC      = $(shell find src/ -name '*.S')

INC       = . $(shell find src/ -type d) HAL/include $(shell find HAL/CPU/$(CPU)/ -type d) $(shell find HAL/BOARD/$(BOARD)/ -type d)

LIBRARIES = c m stdc++

PLATFORM_CSRC   = $(shell find HAL/CPU/$(CPU) -name '*.c')
PLATFORM_CXXSRC = $(shell find HAL/CPU/$(CPU) -name '*.cpp')
PLATFORM_ASRC   = $(shell find HAL/CPU/$(CPU) -name '*.S')
PLATFORM_O      = $(patsubst %.c,$(O)/%.o,$(notdir $(PLATFORM_CSRC))) $(patsubst %.S,$(O)/%.o,$(notdir $(PLATFORM_ASRC))) $(patsubst %.cpp,$(O)/%.o,$(notdir $(PLATFORM_CXXSRC)))
PLATFORM_AR     = $(CPU).ar

LINK_AR   = $(shell find src -name '*.ar') $(O)/$(PLATFORM_AR)

# must be blank, or have trailing slash
#     ok: TOOLCHAIN_PATH =
#     ok: TOOLCHAIN_PATH = C:/Projects/arm-gcc/bin/
# NOT ok: TOOLCHAIN_PATH = C:/Projects/arm-gcc/bin
TOOLCHAIN_PATH  =

ARCH      = arm-none-eabi
PREFIX    = $(ARCH)-

CC        = $(TOOLCHAIN_PATH)$(PREFIX)gcc
CXX       = $(TOOLCHAIN_PATH)$(PREFIX)g++
OBJCOPY   = $(TOOLCHAIN_PATH)$(PREFIX)objcopy
OBJDUMP   = $(TOOLCHAIN_PATH)$(PREFIX)objdump
AR        = $(TOOLCHAIN_PATH)$(PREFIX)ar
SIZE      = $(TOOLCHAIN_PATH)$(PREFIX)size
READELF   = $(TOOLCHAIN_PATH)$(PREFIX)readelf
NM        = $(TOOLCHAIN_PATH)$(PREFIX)nm

# You MUST link with G++ if you have even one C++ source file in the project
# If you have no C++, then feel free to link with gcc which gives a significant reduction in included library code
LINK      = $(TOOLCHAIN_PATH)$(PREFIX)g++

MKDIR     = $(TOOLCHAIN_PATH)mkdir
RMDIR     = $(TOOLCHAIN_PATH)rmdir
RM        = $(TOOLCHAIN_PATH)rm -f

OPTIMIZE  = s

CDEFS    += APPBAUD=$(APPBAUD) DEBUG_MAIN

FLAGS    += -O$(OPTIMIZE)
FLAGS    += -ffunction-sections -fdata-sections
FLAGS    += -Wall -g -funsigned-char
FLAGS    += -funsigned-bitfields -fpack-struct -fshort-enums
FLAGS    += $(patsubst %,-I%,$(INC))
FLAGS    += $(patsubst %,-D%,$(CDEFS))

CFLAGS   += $(FLAGS) -std=gnu99 -pipe -nostdlib -nostartfiles -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vfprintf -fno-builtin-puts

ASFLAGS  += $(FLAGS)

CXXFLAGS += $(FLAGS) -fno-rtti -fno-exceptions -std=gnu++0x

LDFLAGS  += $(FLAGS) -Wl,--as-needed,--gc-sections
LDFLAGS  += -Wl,-Map=$(O)/$(PROJECT).map

LIBS     += $(patsubst %,-l%,$(LIBRARIES))

OBJ       = $(patsubst %,$(O)/%,$(notdir $(CSRC:.c=.o) $(CXXSRC:.cpp=.o) $(ASRC:.S=.o)))

VPATH    = $(O) $(shell find src/ -type d) $(shell find HAL/CPU/$(CPU)/ -type d) $(shell find HAL/BOARD/$(BOARD)/ -type d)

.PHONY: all clean depclean program upload memusage size functions functionsizes

.PRECIOUS: $(OBJ)

all: $(O) $(O)/$(PLATFORM_AR) $(O)/$(PROJECT).elf $(O)/$(PROJECT).bin $(O)/$(PROJECT).hex $(O)/$(PROJECT).lst

clean:
	@echo "  RM    " ".o"
	@$(RM) $(OBJ) $(OBJ:%.o=%.lst)

	@echo "PROTECT " $(O)/$(PLATFORM_AR)
#	@echo "  RM    " "$(PLATFORM_AR)"
#	@$(RM) $(PLATFORM_O) $(PLATFORM_O:%.o=%.lst) $(O)/$(PLATFORM_AR)

	@echo "  RM    " "$(O)/"$(PROJECT)".*"
	@$(RM) $(O)/$(PROJECT).bin $(O)/$(PROJECT).hex $(O)/$(PROJECT).elf $(O)/$(PROJECT).map

depclean:
	@echo "  RM    " $(O)
	@$(RM) -r $(O)

mrproper:
	@echo "  CLEAN " "*"
	@git clean -d -x -f

upload: $(O)/$(PROJECT).bin
	dfu-util -d 1d50:6015 -D $^

debug: $(O)/$(PROJECT).elf
	arm-none-eabi-gdb $< -ex  "set target-charset ASCII" -ex "set remotelogfile mri.log" -ex "target remote $(CONSOLE)"

console:
	stty raw ignbrk -echo $(APPBAUD) < $(CONSOLE)
	( cat <&3 & cat >&3 ; kill %% ) 3<>$(CONSOLE)

memusage: $(O)/$(PROJECT).elf
	@echo " address     size F symbol    file"
	@$(NM) -Sl --size-sort $<
# 	@echo " Object         Address         Size  Source"; grep -A2147483647 'Linker script and memory map' $(O)/$(PROJECT).map | grep -B1 -P '^\s+0x([0-9A-F]{8})\s+0x([0-9A-F]+)\s+.*'

functions: $(O)/$(PROJECT).elf
	@$(READELF) -s $^ | perl -e 'for (<>) { /^\s+(\d+):\s*([0-9A-F]+)\s+(\d+)/i && do { s/^\s+//; push @symbols, [ split /\s+/, $$_ ]; }; }; for (sort { hex($$a->[1]) <=> hex($$b->[1]); } @symbols) { printf "0x%08s: [%4d] %7s %s\n", $$_->[1], $$_->[2], $$_->[3], $$_->[7] if ($$_->[2]) && (hex($$_->[1]) < 0x10000000); }'

functionsizes: $(O)/$(PROJECT).elf
	@$(READELF) -s $^ | perl -e 'for (<>) { /^\s+(\d+):\s*([0-9A-F]+)\s+(\d+)/i && do { s/^\s+//; push @symbols, [ split /\s+/, $$_ ]; }; }; for (sort { $$a->[2] <=> $$b->[2]; } @symbols) { printf "0x%08s: [%4d] %7s %s\n", $$_->[1], $$_->[2], $$_->[3], $$_->[7] if ($$_->[2]) && (hex($$_->[1]) < 0x10000000); }'

$(O):
	@echo "  MKDIR " $@
	@$(MKDIR) $(O)

$(O)/%.bin: $(O)/%.elf
	@echo "  COPY  " $@
	@$(OBJCOPY) -O binary $< $@

$(O)/%.hex: $(O)/%.elf
	@echo "  COPY  " $@
	@$(OBJCOPY) -O ihex $< $@

$(O)/%.lst: $(O)/%.elf
	@echo "  LIST  " $@
	@$(OBJDUMP) -Sg -j.text -j.data -j.rodata $< > $@

$(O)/%.sym: $(O)/%.elf
	@echo "  SYM   " $@
	@$(OBJDUMP) -t $< | perl -ne 'BEGIN { printf "%6s  %-40s %s\n", "ADDR","NAME","SIZE"; } /([0-9a-f]+)\s+(\w+)\s+O\s+\.(bss|data)\s+([0-9a-f]+)\s+(\w+)/ && printf "0x%04x  %-40s +%d\n", eval("0x$$1") & 0xFFFF, $$5, eval("0x$$4")' | sort -k1 > $@

$(O)/%.elf: $(LINK_AR) $(OBJ) HAL/CPU/$(CPU)/$(CPU).ld
	@echo "  LINK  " $@
	@$(LINK) $(LDFLAGS) -Wl,-Map=$(@:.elf=.map) -o $@ $(OBJ) $(LINK_AR) $(LIBS)

$(O)/%.o: %.c | $(O)
	@echo "  CC    " $<
	@$(CC) $(CFLAGS) -Wa,-adhlns=$(@:.o=.lst) -c -o $@ $<

$(O)/%.o: %.cpp | $(O)
	@echo "  CXX   " $<
	@$(CXX) $(CXXFLAGS) -Wa,-adhlns=$(@:.o=.lst) -c -o $@ $<

$(O)/%.o: %.S | $(O)
	@echo "  AS    " $<
	@$(CC) $(ASFLAGS) -Wa,-adhlns=$(@:.o=.lst) -c -o $@ $<

$(O)/$(PLATFORM_AR): $(PLATFORM_O) HAL/CPU/$(CPU)/platform.mk
	@echo "  AR    " $@
	@$(AR) cru $@ $(PLATFORM_O)
	@touch $@

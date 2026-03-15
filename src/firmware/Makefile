# Tyrian Pocket Firmware Makefile
CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
AS = $(CROSS)gcc
LD = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump
SIZE = $(CROSS)size

# Target
TARGET = firmware

# Directories
LIBC_DIR = libc
TYRIAN_DIR = ../../third_party/opentyrian/src

# Common Compiler Flags
COMMON_FLAGS = -march=$(ARCH) -mabi=$(ABI) -mno-relax -ffreestanding -nostdlib -nostartfiles \
               -ffunction-sections -fdata-sections -fno-builtin -fsingle-precision-constant \
               -I. -I$(LIBC_DIR) -Isysinclude -Isdl_shim -I$(TYRIAN_DIR)

# Bootloader/LibC (keep -Os for size, BRAM is 64KB)
CFLAGS = $(COMMON_FLAGS) -Os -Wall -Wextra

# Engine/Platform (use -O3 for max performance, SDRAM is 64MB)
OPT_CFLAGS = $(COMMON_FLAGS) -O3 -Wall -Wextra
TYRIAN_CFLAGS = $(COMMON_FLAGS) -O3 -std=gnu11 -fcommon -DNORMALUNIX -DPOCKET_TYRIAN -DNDEBUG -DTYRIAN_DIR=\".\"
BOOT_SRCS = main.c terminal.c dataslot.c misaligned.c start.S

# Source files - Engine (SDRAM)
LIBC_SRCS = $(LIBC_DIR)/memory.c \
            $(LIBC_DIR)/string.c \
            $(LIBC_DIR)/ctype.c \
            $(LIBC_DIR)/stdlib.c \
            $(LIBC_DIR)/qsort.c \
            $(LIBC_DIR)/time.c \
            $(LIBC_DIR)/math.c \
            $(LIBC_DIR)/file.c \
            $(LIBC_DIR)/setjmp.S

TYRIAN_SRCS = $(TYRIAN_DIR)/animlib.c $(TYRIAN_DIR)/arg_parse.c $(TYRIAN_DIR)/backgrnd.c \
              $(TYRIAN_DIR)/config.c $(TYRIAN_DIR)/config_file.c $(TYRIAN_DIR)/destruct.c \
              $(TYRIAN_DIR)/editship.c $(TYRIAN_DIR)/episodes.c $(TYRIAN_DIR)/file.c \
              $(TYRIAN_DIR)/font.c $(TYRIAN_DIR)/fonthand.c $(TYRIAN_DIR)/game_menu.c \
              $(TYRIAN_DIR)/helptext.c $(TYRIAN_DIR)/jukebox.c $(TYRIAN_DIR)/keyboard.c $(TYRIAN_DIR)/lds_play.c \
              $(TYRIAN_DIR)/loudness.c $(TYRIAN_DIR)/lvllib.c $(TYRIAN_DIR)/lvlmast.c \
              $(TYRIAN_DIR)/mainint.c $(TYRIAN_DIR)/menus.c $(TYRIAN_DIR)/mouse.c $(TYRIAN_DIR)/mtrand.c \
              $(TYRIAN_DIR)/musmast.c $(TYRIAN_DIR)/network.c $(TYRIAN_DIR)/nortsong.c \
              $(TYRIAN_DIR)/nortvars.c $(TYRIAN_DIR)/opentyr.c $(TYRIAN_DIR)/palette.c \
              $(TYRIAN_DIR)/params.c $(TYRIAN_DIR)/pcxload.c $(TYRIAN_DIR)/pcxmast.c \
              $(TYRIAN_DIR)/picload.c $(TYRIAN_DIR)/player.c $(TYRIAN_DIR)/shots.c \
              $(TYRIAN_DIR)/sizebuf.c $(TYRIAN_DIR)/sndmast.c $(TYRIAN_DIR)/sprite.c \
              $(TYRIAN_DIR)/starlib.c $(TYRIAN_DIR)/tyrian2.c $(TYRIAN_DIR)/varz.c \
              $(TYRIAN_DIR)/vga256d.c $(TYRIAN_DIR)/vga_palette.c $(TYRIAN_DIR)/xmas.c

PLATFORM_SRCS = tyrian_pocket.c sdl_pocket.c

# Compilation
ARCH = rv32imaf
ABI = ilp32f

# Global flags
COMMON_FLAGS = -march=$(ARCH) -mabi=$(ABI) -mno-relax -ffreestanding -nostdlib -nostartfiles \
               -ffunction-sections -fdata-sections -fno-builtin -fsingle-precision-constant \
               -I. -I$(LIBC_DIR) -Isysinclude -Isdl_shim -I$(TYRIAN_DIR)

CFLAGS = $(COMMON_FLAGS) -Os -Wall -Wextra
ASFLAGS = -march=$(ARCH) -mabi=$(ABI) -mno-relax
LDFLAGS = -march=$(ARCH) -mabi=$(ABI) -mno-relax -T linker.ld -nostdlib -nostartfiles \
          -Wl,--gc-sections -Wl,-Map=$(TARGET).map

# Tyrian-specific flags
TYRIAN_CFLAGS = $(COMMON_FLAGS) -O2 -std=gnu11 -fcommon -DNORMALUNIX -DPOCKET_TYRIAN -DNDEBUG -DTYRIAN_DIR=\".\"

# Objects
BOOT_OBJS = main.o terminal.o dataslot.o misaligned.o start.o
LIBC_OBJS = $(LIBC_DIR)/memory.o $(LIBC_DIR)/string.o $(LIBC_DIR)/ctype.o $(LIBC_DIR)/stdlib.o \
            $(LIBC_DIR)/qsort.o $(LIBC_DIR)/time.o $(LIBC_DIR)/math.o $(LIBC_DIR)/file.o $(LIBC_DIR)/setjmp.o
ENGINE_OBJS = $(notdir $(TYRIAN_SRCS:.c=.o)) $(PLATFORM_SRCS:.c=.o)

ALL_OBJS = $(BOOT_OBJS) $(LIBC_OBJS) $(ENGINE_OBJS)

# Libs
LIBGCC = $(shell $(CC) -march=$(ARCH) -mabi=$(ABI) -print-libgcc-file-name)

all: $(TARGET).mif tyrian.bin
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(ALL_OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS) $(LIBGCC)

$(TARGET).mif: $(TARGET).elf
	$(OBJCOPY) -O binary -j .boot -j .boot_data -j .boot_bss -j .fasttext -j .fastdata $< boot.bin
	@echo "WIDTH=32;" > $@
	@echo "DEPTH=16384;" >> $@
	@echo "ADDRESS_RADIX=DEC;" >> $@
	@echo "DATA_RADIX=HEX;" >> $@
	@echo "" >> $@
	@echo "CONTENT BEGIN" >> $@
	@hexdump -v -e '1/4 "%08X\n"' boot.bin | awk '{printf "    %d : %s;\n", NR-1, $$1}' >> $@
	@LAST=$$(hexdump -v -e '1/4 "%08X\n"' boot.bin | wc -l); \
	if [ $$LAST -lt 16384 ]; then echo "    [$$LAST..16383] : 00000013;" >> $@; fi
	@echo "END;" >> $@

tyrian.bin: $(TARGET).elf
	$(OBJCOPY) -O binary --only-section=.text --only-section=.data $< $@

# Rules
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

$(LIBC_DIR)/%.o: $(LIBC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIBC_DIR)/%.o: $(LIBC_DIR)/%.S
	$(AS) $(ASFLAGS) -c -o $@ $<

# Engine objects (from external dir)
$(filter-out tyrian_pocket.o sdl_pocket.o, $(ENGINE_OBJS)): %.o: $(TYRIAN_DIR)/%.c
	$(CC) $(TYRIAN_CFLAGS) -c -o $@ $<

tyrian_pocket.o: tyrian_pocket.c
	$(CC) $(OPT_CFLAGS) -c -o $@ $<

sdl_pocket.o: sdl_pocket.c
	$(CC) $(OPT_CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(LIBC_DIR)/*.o $(TARGET).elf $(TARGET).bin $(TARGET).mif $(TARGET).map $(TARGET).lst
	rm -f boot.bin tyrian.bin

.PHONY: all clean rebuild

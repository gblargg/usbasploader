include bootloaderconfig.inc
include devices.inc

ifdef DEBUG_AS_APP
    CFLAGS  += -DNO_FLASH_WRITE
    CFLAGS  += -DDEBUG_LEVEL=1
else
	BLFLAGS += -Wl,--section-start=.text=$(BOOTLOADER_ADDRESS)
endif

CFLAGS  += -Wall
CFLAGS  += -DBOOTLOADER_ADDRESS=$(BOOTLOADER_ADDRESS)
CFLAGS  += -Os
CFLAGS  += -fno-jump-tables # avoid implicit LPM instructions that would break on >64K flash
# These give significant code size reduction
CFLAGS  += -fno-move-loop-invariants -fno-tree-scev-cprop -fno-inline-small-functions
LDFLAGS += -Wl,--relax,--gc-sections

SOURCES += usbdrv/usbdrvasm.S
SOURCES += usbdrv/oddebug.c
SOURCES += main.c

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)

all: hex
	@avr-size obj/main.bin
	@avr-objdump -d obj/main.bin > obj/main.lss

settings:
	@echo BOOTLOADER_ADDRESS = $(BOOTLOADER_ADDRESS)
	@echo FUSEOPT = $(FUSEOPT)

hex:
	@-mkdir obj 2>/dev/null || true
	@avr-gcc -mmcu=$(DEVICE) -DF_CPU=$(F_CPU) $(CFLAGS) $(LDFLAGS) $(BLFLAGS) \
		-o obj/main.bin -I. $(SOURCES)
	@avr-objcopy -R .eeprom -R .fuse -R .lock -R .signature -O ihex obj/main.bin obj/main.hex

flash: hex
	$(AVRDUDE) -U flash:w:obj/main.hex

fuse:
	$(AVRDUDE) $(FUSEOPT)

# Self-update bootloader
update: hex
	@if test $(DEBUG_AS_APP); then echo "DEBUG_AS_APP must not be enabled when updating"; exit 1; fi
	@# Make raw binary of bootloader and add to updater as an array in program memory
	@avr-objcopy -j .text -j .data -O binary obj/main.bin obj/loader.raw
	@hexdump -v -e '1/1 "0x%02x,\n"' obj/loader.raw > obj/loader.h
	@avr-gcc -Wall -mmcu=$(DEVICE) -DF_CPU=$(F_CPU) $(CFLAGS) $(LDFLAGS) \
		-o obj/update.bin -I. update.c
	@avr-objdump -d obj/update.bin > obj/update.lss
	@avr-objcopy -j .text -j .data -O ihex obj/update.bin obj/update.hex
	$(AVRDUDE) -U flash:w:obj/update.hex

clean:
	@-rm obj/*

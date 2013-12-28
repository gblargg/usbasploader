# Sets BOOTLOADER_ADDRESS and FUSES based on DEVICE

# Author: Stephan Bärwolf, Shay Green
# License: GNU GPL v2 (see License.txt)

# Untested

FUSEOPT_8535            = -U lfuse:w:0x1f:m -U hfuse:w:0xc0:m
BOOTLOADER_ADDRESS_8535 = 0x1800

FUSEOPT_16              = -U lfuse:w:0x1f:m -U hfuse:w:0xc0:m
BOOTLOADER_ADDRESS_16   = 0x3800

FUSEOPT_88              = -U lfuse:w:0xd7:m -U hfuse:w:0xd4:m -U efuse:w:0xf8:m
BOOTLOADER_ADDRESS_88   = 0x1800

FUSEOPT_164             = -U lfuse:w:0xd7:m -U hfuse:w:0xd0:m -U efuse:w:0xfc:m
BOOTLOADER_ADDRESS_164  = 0x3800

FUSEOPT_324             = $(FUSEOPT_164)
BOOTLOADER_ADDRESS_324  = 0x7000

FUSEOPT_128             = -U lfuse:w:0x1f:m -U hfuse:w:0xc0:m -U efuse:w:0xfd:m
BOOTLOADER_ADDRESS_128  = 0x1E000

FUSEOPT_640             = -U lfuse:w:0xd7:m -U hfuse:w:0xd0:m -U efuse:w:0xfc:m
BOOTLOADER_ADDRESS_640  = 0xE000

FUSEOPT_1280             = $(FUSEOPT_640)
BOOTLOADER_ADDRESS_1280  = 0x1E000

FUSEOPT_1281             = $(FUSEOPT_1280)
BOOTLOADER_ADDRESS_1281  = 0x1E000

FUSEOPT_2561             = $(FUSEOPT_1281)
BOOTLOADER_ADDRESS_2561  = 0x3E000

# Tested

FUSEOPT_8            = -U lfuse:w:0x9f:m -U hfuse:w:0xc0:m
BOOTLOADER_ADDRESS_8 = 0x1800

FUSEOPT_32              = $(FUSEOPT_16)
BOOTLOADER_ADDRESS_32   = 0x7000

FUSEOPT_168             = $(FUSEOPT_88)
BOOTLOADER_ADDRESS_168  = 0x3800

FUSEOPT_328             = -U lfuse:w:0xd7:m -U hfuse:w:0xd0:m -U efuse:w:0x04:m
BOOTLOADER_ADDRESS_328  = 0x7000

FUSEOPT_644             = $(FUSEOPT_324)
BOOTLOADER_ADDRESS_644  = 0xE000

FUSEOPT_1284            = -U lfuse:w:0xd7:m -U hfuse:w:0xd0:m -U efuse:w:0xfc:m
BOOTLOADER_ADDRESS_1284 = 0x1E000

FUSEOPT_2560             = $(FUSEOPT_1280)
BOOTLOADER_ADDRESS_2560  = 0x3E000

# Combine number from device with above defines, e.g. for atmega8, FUSEOPT = FUSEOPT_8

# Strip number from atmegaNN[p][a] e.g. atmega8=8, atmega16pa=16
DEVICE_NUM0 = $(patsubst atmega%,%,$(DEVICE))
DEVICE_NUM1 = $(patsubst %a,%,$(DEVICE_NUM0))
DEVICE_NUM = $(patsubst %p,%,$(DEVICE_NUM1))

ifndef FUSEOPT
    FUSEOPT = $($(patsubst %,FUSEOPT_%,$(DEVICE_NUM)))
endif

ifndef BOOTLOADER_ADDRESS
    BOOTLOADER_ADDRESS = $($(patsubst %,BOOTLOADER_ADDRESS_%,$(DEVICE_NUM)))
endif

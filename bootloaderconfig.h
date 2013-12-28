// Required: port and bits used for both USB data lines (D+ must also connect to INT0)
#define USB_CFG_IOPORTNAME  B
#define USB_CFG_DMINUS_BIT  0
#define USB_CFG_DPLUS_BIT   1

// Nothing more is required in this file. Everything else is optional and customizes options.

// Without any configuration options below this, the bootloader will run after any kind of reset
// and wait indefinitely until avrdude connects. Once avrdude disconnects, the user
// program gets run. Override by copying configuration lines from bootloaderconfig-palette.h

// Bootloader runs when powered up/reset. Exits if avrdude doesn't connect after a few seconds.
#define BOOTLOADER_ON_POWER 1

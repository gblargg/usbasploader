// Copy these to your bootloaderconfig.h to have the indicated effect

//**** Bootloader entry

// At most only one should be defined

// Bootloader runs when powered up/reset. Exits if avrdude doesn't connect after a few seconds.
#define BOOTLOADER_ON_POWER 1

// Bootloader runs only when USB is connected, then waits indefinitely for avrdude to connect.
#define BOOTLOADER_ON_USB 1

// Bootloader runs only when reset is triggered externally (e.g. a reset button).
#define BOOTLOADER_ON_RESET 1

// Bootloader runs only when watchdog reset occurs.
#define BOOTLOADER_ON_WDT 1

// Bootloader runs only when jumper is pulled low while device is powered on/reset.
// Set the port and pin/bit the jumper is on.
#define BOOTLOADER_ON_JUMPER 1
#define BOOTLOADER_JUMPER_PORT B
#define BOOTLOADER_JUMPER_BIT  2


//**** Bootloader exit

// These can be combined with the above entry options.

// Flash LED connected to specified port and pin/bit. LED must go to +V, not GND.
#define LED_PRESENT 1
#define LED_PORT C
#define LED_BIT  1

// Have bootloader auto-exit if USB isn't connected.
#define AUTO_EXIT_NO_USB 1

// Have bootloader auto-exit after this many milliseconds if avrdude hasn't connected.
#define AUTO_EXIT_MS 4000

// Keep bootloader from automatically exiting on its own, only being able to exit
// if bootLoaderCondition() is false.
#define BOOTLOADER_CAN_EXIT 0


//**** Configuration

// Override default chip signature
#define SIGNATURE_BYTES 0x12, 0x34, 0x56, 0


//**** Options

// Use full chip erase rather than on-demand page erase. Erases all
// pages rather than just those rewritten.
#define HAVE_CHIP_ERASE 1

// Prevent bootloader from being able to self-update to a different version
#define HAVE_SELF_UPDATE 0


//**** Code size reduction

// Least-important features listed first

#define HAVE_READ_LOCK_FUSE         0 // Disable read fuse bytes
#define HAVE_FLASH_BYTE_READACCESS  0 // Disable read individual flash bytes
#define HAVE_EEPROM_BYTE_ACCESS     0 // Disable read/write individual eeprom bytes
#define HAVE_EEPROM_PAGED_ACCESS    0 // Disable upload/download eeprom
#define HAVE_FLASH_PAGED_READ       0 // Disable download flash


//**** Bootloader entry/exit customization

// If customizing any of these, all three must be defined.

// Called at beginning of main(). OK to call leaveBootloader() to run user
// progam immediately. Can examine MCUCSR to find cause of reset.
static void bootLoaderInit()
{
	// ...
}

// Called from bootloader's main loop. If false, exits loop. Must be macro.
#define bootLoaderCondition() (1)

// Called just before running user program. OK to use custom method of running
// user program here, instead of returning.
static void bootLoaderExit()
{
	// ...
}

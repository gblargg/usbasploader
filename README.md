USBaspLoader: USBasp-Compatible Bootloader for AVR
==================================================
This is an experimental version and hasn't been tested on much besides an atmega8.

USBaspLoader is a bootloader that allows most atmega devices to be reprogrammable via USB. The device works normally until some configurable event occurs (button pressed, device is reset, etc.). Then it goes into USBasp mode where the main program can be reflashed using avrdude on the host machine. Afterwards it can immediately run the new program.

Based on Stephan Baerwolf's improved USBaspLoader, started by Christian Starkjohann, based on Thomas Fischl's USBasp programmer and AVRUSBBoot bootloader. My main goal is clear, robust, maintainable code with a minimum of assembly.

Licensed under GNU GPL v2. See License.txt.

Sections:

* Features
* Requirements
* Configuration
* Bootloader entry/exit
* Bootloader custom entry/exit
* Self-update
* Code size
* Automatic device configuration
* Differences from original USBaspLoader
* Design
* Development


Features
--------
* Gives device a USBasp interface so it can be reprogrammed with avrdude.

* Flexible customization of how bootloader is invoked.

* Can self-update USBaspLoader boot code itself, allowing field updates of the whole device.

* Only needs minimal hardware for USB interface: a few resistors, and two zener diodes if the device doesn't run at 3.3V.

* Optionally supports individual byte writing/reading and dumping flash, eeprom, and fuses. Useful when examining memory during development.


Requirements
------------
* Atmega chip with bootloader support and 2K flash space. For attiny chips, use the micronucleus bootloader.

* Free pins suitable for V-USB (two inputs on the same port, with one of them or a third free pin supporting interrupt/pin-change interrupt).

* 12MHz, 15MHz, 16MHz, 18MHz, or 20MHz crystal oscillator, and possibly 12.8MHz or 16.5MHz internal oscillator-based clocks.


Files
-----
    usbdrv/                         V-USB library
    bootloaderconfig.h              Configuration/customization; modify as needed
    bootloaderconfig-palette.h      Available configuration options; copy from this
    bootloaderconfig.inc            More configuration; modify as needed
    devices.inc                     Auto-configuration; don't modify
    do_spm.c                        Self-update core routine
    License.txt                     GNU GPL v2
    main.c                          USBaspLoader code
    Makefile                        Builds program; don't modify
    postconfig.h                    Internal configuration
    Readme.md                       Documentation
    update.c                        Self-updater program
    usbconfig.h                     V-USB configuration; don't modify


Setup
-----
* Refer to V-USB documentation for how to build the USB hardware interface.

* Configure the code as described below.

* Configure your programmer with PROGRAMMER in bootloaderconfig.inc. avrdude is assumed as the programming software.

* Connect the device to the programmer. Set the fuses with

        make fuse

  then flash the bootloader with

        make flash

* Disconnect the programmer and connect the device to USB.

* Reset the device with whatever jumper or invocation conditions you've set up. The default runs the bootloader for four seconds at power/reset.

* Use avrdude to flash a known-working program to the device.


Configuration
-------------
Your hardware setup must be configured. This consists of the AVR chip model, clock frequency, and the USB connection pins. Everything to edit is in bootloaderconfig.h and bootloaderconfig.inc. By default it comes configured to work on the common USBasp programmer.

Set the AVR chip model and clock speed in bootloaderconfig.inc. The rest should be set automatically.

USB configuration is just which port and pins USB D+ and D- are connected to, and interrupt configuration if something other than INT0 is being used. It also uses the F_CPU setting for CPU clock, which must be one of the supported rates. See usbdrv/Readme.txt and usbdrv/usbconfig-prototype.h "Hardware Config" section for more.

How the bootloader is started and when it exits can be customized. The default bootloaderconfig.h has the bootloader wait for a few seconds for avrdude. If there's no activity, it runs the user program.

The bootloader has several features that can be disabled in order to help it fit within the common 2K limit for a bootloader. This is only necessary if it won't build due to being too large.


Bootloader entry/exit
---------------------
Without any configuration, the bootloader will run on reset and wait indefinitely until avrdude connects. Once avrdude disconnects, the user program gets run. See bootloaderconfig-palette.h for more configuration options. Listed are several common ways the bootloader might be configured to run, with the configuration shown after.

* No button/jumper w/ USB: When turned on, immediately enters USBasp mode. If no activity for 4 seconds, automatically exits and runs program. Good when your program also uses USB.

        #define BOOTLOADER_ON_POWER 1

* No button/jumper: Same as above but only when USB is connected; otherwise your program runs immediately. Good when your program doesn't use USB.

        #define BOOTLOADER_ON_USB 1

* No button/jumper: When turned on, runs program normally. When you do something defined by the program, it directly runs the bootloader.

        #define BOOTLOADER_ON_WDT 1
        
        // when you want to enter bootloader from your program:
        cli();
        wdt_enable( WDTO_15MS );
        while ( 1 ) { }

* Reset button: When turned on, runs program normally. When reset button is pressed, enters USBasp mode.

        #define BOOTLOADER_ON_RESET 1

* Jumper: When started without jumper, runs program normally. When started with jumper in place, enters USBasp mode.

        #define BOOTLOADER_ON_JUMPER 1
        #define BOOTLOADER_JUMPER_PORT B
        #define BOOTLOADER_JUMPER_BIT 2


Bootloader custom entry/exit
----------------------------
Entry/exit can be further customized with three macros that are run before, during, and after the bootloader. They are invoked in the following structure:

    int main( void )
    {
        bootLoaderInit(); // might decide to call leaveBootloader()
    
        _delay_ms( 260 );
    
        while ( bootLoaderCondition() )
        {
            run_bootloader();
            
            #if BOOTLOADER_CAN_EXIT
                if ( avrdude_exited )
                    break;
                
                #ifdef AUTO_EXIT_MS
                    if ( elapsed_milliseconds >= AUTO_EXIT_MS &&
                            !avrdude_connected )
                        break;
                #endif
            #endif
        }
    
        leaveBootloader();
    }

    void leaveBootloader( void )
    {
        bootLoaderExit();
        runUserApp();
    }

When the device is powered on or reset, the bootloader's reset code runs, not the normal program's. The first thing it does is run your bootLoaderInit(). Your bootLoaderInit() should initialize any hardware necessary to tell what to do. For a jumper, this might mean setting its input to have a weak pull-up. If you can already determine whether to run the bootloader, you can check it now and call leaveBootLoader() if you want the user program to run. For example, when running the bootloader after any reset, just examine the MCUCSR for the EXTRF bit being set.

After bootLoaderInit() is run, hundreds of milliseconds of initialization will be done (USB reset). This gives any pull-ups for jumpers time to settle, so that they can be read. After this delay, bootLoaderCondition() is called, and if true, the bootloader is run, otherwise the user program is run.

While the bootloader is running, bootLoaderCondition() is called repeatedly and if it ever returns false, the bootloader is exited immediately. In addition, avrdude connecting then exiting will exit the loop, and AUTO_EXIT_MS milliseconds passing without avrdude connecting will also exit the loop.

Once the bootloader is about to run the user program (no matter what path it took to get there), it calls your bootLoaderExit(). This is where you can restore any hardware settings you configured in bootLoaderInit(), for example disable the pullups you enabled before. Further, you can then optionally use your own approach to running the user program, or just return and let the bootloader run it by jumping to zero.

To customize these, bootLoaderCondition must be defined as a macro. bootLoaderInit() and bootLoaderExit() can be functions or macros. If functions, they should be declared static. This is shown in the example below.

The following example invokes the bootloader only when a jumper on port B2 is tied to ground and the reset button is pressed. It also lights an LED on port C0 while the bootloader is running and turns it off when the user program begins. Unlike with external reset, we can't read the jumper immediately since it takes some time to give a valid input. So we wait to check it in bootLoaderCondition() which is invoked after a delay. If the jumper isn't set by then, the bootloader's loop never runs so it just exits immediately.

    static void bootLoaderInit( void )
    {
        if ( !(MCUCSR & (1<<EXTRF)) )
            leaveBootloader(); // wasn't external reset, so run user program
        
        PORTB |= 1<<2; // weak pull-up on jumper
    }
    
    // Run bootloader while jumper pin is pulled low
    #define bootLoaderCondition() ((PINB & (1<<2)) == 0)
    
    static void bootLoaderExit( void )
    {
        PORTB &= ~(1<<2); // restore to original state as input
    }


Self-update
-----------
The bootloader can update itself by running

    make update

The updater first checks to see whether the new bootloader even differs from the current one; if the same, it skips the reflashing step. After flashing, the updater verifies that the new bootloader was written successfully. If unsuccessful, the updater will go into an endless loop. If successful or the bootloader was already updated, the updater performs a watchdog reset which, depending on your configuration, might re-enter the new bootloader.


Code size
---------
Many devices only give 2K of flash for a bootloader, and this bootlaoder comes close to that. On some configurations/compilers, it may exceed that. On gcc, the error message is something like

    address 0xhhhh of main.bin section `.text' is not within region `text'

Clock speed affects code size; 15MHz and especially 16.5MHz generate more code, and 12.8Hz's code won't fit on devices with only a 2K bootloader, even with only minimal features enabled.

Many features are not essential for basic uploading functionality and can be disabled. By default the code attempts to disable some if necessary, though it might fail where manual adjustment could succeed. To override these defaults, uncomment features and change to 0 or 1 as desired. They are listed here from least to most essential. Configure in bootloaderconfig.h.

* HAVE_READ_LOCK_FUSE: Support for reading fuse bytes. avrdude examines these but they aren't important normally.

* HAVE_FLASH_BYTE_READACCESS: Support for reading individual flash bytes, used in avrdude's interactive terminal mode.

* HAVE_EEPROM_BYTE_ACCESS: Support for reading and writing individual eeprom bytes, used in avrdude's interactive terminal mode.

* HAVE_EEPROM_PAGED_ACCESS: Support for uploading/downloading eeprom. This is important if your program includes eeprom data it uses.

* HAVE_CHIP_ERASE: Support for erasing entire device's flash memory (other than bootloader). When disabled (the default), and avrdude has requested a chip erase, flash memory is erased incrementally as a program is uploaded, and any flash beyond the program is left unerased.

* BOOTLOADER_CAN_EXIT: Support for exiting the bootloader automatically and running the user program when avrdude is done. When disabled, the the user-defined condition (closed jumper, etc.) must be cleared, or the device must be reset, to run the user program.


Automatic device configuration
------------------------------
Some configuration is specific to the device type. Common device types have this automatically configured ("make settings" shows current ones). The following describes how to manually configure them.

The bootloader sits at the top of flash memory and has its own interrupt vector table. A given device requires that this be at a certain address. Fuses must also be set to enable running the bootloader at reset and to set its size. The following describes how these are found for a typical device (atmega8).

First, find fuse bits documentation. For the atmega8, we have the following (remember that a bit set to 1 means that the feature is DISABLED, and 0 means ENABLED):

    Table 87. Fuse High Byte
                    
    Name      Bit   Default   Function
    RSTDISBL  7     1         Select if PC6 is I/O pin or RESET pin
    WDTON     6     1         WDT always on
    SPIEN     5     0         Enable Serial Program and Data 0
    CKOPT     4     1         Oscillator options
    EESAVE    3     1         EEPROM memory is preserved through the Chip Erase
    BOOTSZ1   2     0         Select Boot Size
    BOOTSZ0   1     0         Select Boot Size
    BOOTRST   0     1         Select Reset Vector

(fuse low byte doesn't have any relevant bits)

    Table 82. Boot Size Configuration
    
    BOOTSZ1     BOOTSZ0     Size        Boot Reset Address
    1           1           128 words   0xF80
    1           0           256 words   0xF00
    0           1           512 words   0xE00
    0           0           1024 words  0xC00

The boot sizes and addresses are shown in words and must be translated into bytes by multiplying by 2. We want BOOTSZ1 and BOOTSZ0 both set to 0, giving 1024 x 2 = 2048 bytes for the bootloader. This puts it at byte address 0xC00 x 2 = 0x1800.

We also want BOOTRST enabled so that the bootloader's reset vector is used rather than the user program's.  So we want to clear the low 3 bits of the fuse high byte. Since we're using an external crystal, CKOPT will also be clear, so the fuse high byte will be 0xc8.

In bootloaderconfig.inc:

    BOOTLOADER_ADDRESS = 0x1800
    FUSEOPT = -U hfuse:w:0xc8:m

You might also want to set the lfuse, which depends on your external crystal source.


Notes
-----
* Works as great addition to a USBasp ISP programmer stick. Put this on as bootloader and then stick can self-update to a newer version of the USBasp software, or other programming protocol. The USBasp stick is also a cheap platform for developing small projects, possibly V-USB based, so you can have a bunch of these around with USBaspLoader as the bootloader, ready to be self-programmed with whatever.

* This project was inspired by Thomas Fischl's AVRUSBBoot, which used a custom protocol not compatible with avrdude. This lead to Objective Development's (Christian Starkjohann) USBaspLoader, which uses the same protocol as USBasp. Stephan Baerwolf extended USBaspLoader to support more of USBasp's features, fix some bugs, configure automatically for many devices, and optimize many things in assembly. I (Shay Green) back-ported Stephan Baerwolf's improvements and bug fixes to the base USBaspLoader codebase, added a few features, reduced code size, and worked on making the code clear and readable.


Differences from original USBaspLoader
--------------------------------------
These features were added or differ from the original usbasploader.2012-12-08 source:

* Works even when watchdog timer is fused on.
* Calls bootLoaderExit() just before running user program, rather than before doing final cleanup, this way bootLoaderExit() can invoke user program its own way if desired.
* Fixes code to work on >64K flash devices (untested).
* Allows disabling more non-essential features, including flash dumping.
* Allows flashing code without erasing first, which is done with the -D option on avrdude.
* Acknowledges USBASP_FUNC_SETISPSCK command to avoid spurious warning from avrdude.
* Supports fuse and lock reading, and single-byte flash reading.
* Automatically configures for several more atmega devices.
* Uses software-based protection from overwriting bootloader; doesn't need hardware lock fuse support.
* Verifies CRC of received USB data before writing to flash.


To do
-----
* Way to confirm that self-update of bootloader was successful.


Design
------
* Chip erase stopping at the bootloader: there's no way the bootloader can erase itself without jumping through hoops. If the chip erase were to go to the end of flash, it would erase the loop itself before then, unless we arranged for the erase loop 

* Self-update is non-trivial because some chips prevent reflashing from a program not running in the bootloader area at the top of flash. To work around this, a small routine that executes the flash commands is included with the bootloader, and then the updater calls this to do reflashing. One further complication is that the page(s) containing this routine can't be reflashed by that routine since it would erase itself in the middle of the process, so a second copy of this routine is put at the end of flash, and the two used in combination to reflash the entire bootloader area.

* To allow the updater to more easily find the self-update routine in the bootloader, we make it the SPM_RDY_vect handler. Most/all devices that support self-programming have this vector. Then the upder just needs to invoke that vector in the bootloader. The alternative would be modifying the startup code to put the handler at a fixed address somewhere after the vectors.

* Large flash handling has three issues to handle:

- If BOOTLOADER_ADDRESS > 0x1ffff (that is, more than 128k of flash), EIND needs to be set. avr-gcc 4.5.3 at least sets it properly at startup to 0/1 depending on code address. And then when we jump to address 0, we must set EIND to 0 as compiler doesn't before it issues EICALL (compiler assumes code doesn't cross 128K flash boundaries).

- usbdrv reads configuration data from flash, so needs to know whether to use pgm_read_byte() or pgm_read_byte_far(), set via USB_CFG_DRIVER_FLASH_PAGE in usbconfig.h.

- When there's more than 64K flash, our flash read/write code needs to use more than 16 bits for the address and call pgm_read_byte_far() for reads. For writing, apparently the boot_page_*() functions take appropriate 16/32-bit addresses depending on the device compiled for, so no special handling is needed for these.

* Updater previously used avr-objcopy to convert loader.raw into loader.o and then link with updater.c, but this required figuring out the AVR architecture name (e.g. avr3) for -B to avr-objcopy and other parameters that required knowing details about the implementation. Now it just dumps loader.raw to loader.h and includes as a C array of hex bytes. Simpler and less brittle.


Development
-----------
If working on the bootloader itself, it has some features to help with debugging. In bootloaderconfig.inc, enable NO_FLASH_WRITE. This puts the bootloader at 0 as a normal program so that it can be tested without having to erase the bootloader on the test device (which might be another working copy of USBaspLoader). This avoids the problem of having two USBasp programmers connected at the same time, and how to tell avrdude which one is the "real" one.

Running the code from address 0 gives ample space for the code and debugging features which expand it. It disables actual flash writing, so that operations can be tested without overwriting anything. It enables debug output on the serial port; DBGn( k, p, n ) throughout the code will print, in hex, k, then n bytes pointed to by p, then a newline. These can be used to see execution and state.

After enabling NO_FLASH_WRITE and flashing the device, run the same flash command again to verify basic functionality and read verification. Even though it won't reflash, it will read flash back and verify all data, which should match since it was just actually flashed before this. This tests most of the functionality of the loader. You can also invoke avrdude -t to enter interactive terminal mode to test more features (lock fuses, eeprom access). When ready to test a new revision, just run the bootloader and upload it. This allows easy edit-debug testing of most functionality without a second programmer.

-- 
Shay Green <gblargg@gmail.com>

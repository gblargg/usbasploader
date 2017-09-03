// Misc configuration here rather than source to avoid cluttering it with non-code

#if BOOTLOADER_ADDRESS % SPM_PAGESIZE != 0
	#error "BOOTLOADER_ADDRESS must be on page boundary"
#endif

// Auto-disable features if only 2K bootloader space
#if (FLASHEND - BOOTLOADER_ADDRESS) <= 0x800
	#if !defined (HAVE_READ_LOCK_FUSE) && (USB_CFG_CLOCK_KHZ == 15000 || \
			USB_CFG_CLOCK_KHZ == 16500 || USB_CFG_CLOCK_KHZ == 12800)
		#warning "Disabling HAVE_READ_LOCK_FUSE to fit code budget"
		#define HAVE_READ_LOCK_FUSE 0
	#endif
	#if !defined (HAVE_FLASH_BYTE_READACCESS) && \
			(USB_CFG_CLOCK_KHZ == 16500 || USB_CFG_CLOCK_KHZ == 12800)
		#warning "Disabling HAVE_FLASH_BYTE_READACCESS to fit code budget"
		#define HAVE_FLASH_BYTE_READACCESS 0
	#endif
	#if !defined (HAVE_EEPROM_BYTE_ACCESS) && USB_CFG_CLOCK_KHZ == 12800
		#warning "Disabling HAVE_EEPROM_BYTE_ACCESS to fit code budget"
		#define HAVE_EEPROM_BYTE_ACCESS 0
	#endif
	#if !defined (HAVE_EEPROM_PAGED_ACCESS) && USB_CFG_CLOCK_KHZ == 12800
		#warning "Disabling HAVE_EEPROM_PAGED_ACCESS to fit code budget"
		#define HAVE_EEPROM_PAGED_ACCESS 0
	#endif
	#if !defined (HAVE_FLASH_PAGED_READ) && USB_CFG_CLOCK_KHZ == 12800
		#warning "Disabling HAVE_FLASH_PAGED_READ to fit code budget"
		#define HAVE_FLASH_PAGED_READ 0
	#endif
	#if !defined (BOOTLOADER_CAN_EXIT) && USB_CFG_CLOCK_KHZ == 12800
		#warning "Disabling BOOTLOADER_CAN_EXIT to fit code budget"
		#define BOOTLOADER_CAN_EXIT 0
	#endif
	#if !defined (HAVE_SELF_UPDATE) && USB_CFG_CLOCK_KHZ == 12800
		#define HAVE_SELF_UPDATE 0
	#endif
#endif

//**** Compatibility with other bootloaders

// Support AVRUSBBoot's bootloaderconfig.h
#ifdef BOOTLOADER_CONDITION
	#define bootLoaderCondition() BOOTLOADER_CONDITION
#endif

#ifdef BOOTLOADER_INIT
	#define bootLoaderInit() BOOTLOADER_INIT
	#define bootLoaderExit()
	#ifndef bootLoaderCondition
		#define bootLoaderCondition() 1
	#endif
#endif

#if HAVE_ONDEMAND_PAGEERASE
	#undef HAVE_CHIP_ERASE
#endif

//**** Debugging support

#if NO_FLASH_WRITE
	#define SET_IVSEL( b )
	
	#undef boot_page_erase
	#define boot_page_erase( addr ) ((void) 0)
	
	#undef boot_page_write
	#define boot_page_write( addr ) ((void) 0)
#else
	#ifndef GICR
		#define GICR MCUCR
	#endif
	
	// If b=1, use bootloader's interrupt vectors
	#define SET_IVSEL( b ) (GICR = 1<<IVCE, GICR = (b)<<IVSEL)
#endif

//**** Entry/exit

#if BOOTLOADER_ON_POWER
	#define AUTO_EXIT_NO_USB 1
	#define AUTO_EXIT_MS 4000
#endif

#if BOOTLOADER_ON_USB
	#define AUTO_EXIT_NO_USB 1
#endif

#if BOOTLOADER_ON_RESET
	static void bootLoaderInit( void ) { if ( !(MCUCSR & (1<<EXTRF)) ) leaveBootloader(); }
	#define bootLoaderCondition() 1
	static void bootLoaderExit( void ) { }
#endif

#if BOOTLOADER_ON_WDT
	static void bootLoaderInit( void ) { if ( !(MCUCSR & (1<<WDRF)) ) leaveBootloader(); }
	#define bootLoaderCondition() 1
	static void bootLoaderExit( void ) { }
#endif

#if BOOTLOADER_ON_JUMPER
	// All macros to delay of not-yet-defined USB_OUTPORT()/USB_INPORT()
	#define bootLoaderInit() \
			{ USB_OUTPORT(BOOTLOADER_JUMPER_PORT) |= 1<<BOOTLOADER_JUMPER_BIT; }
	
	#define bootLoaderCondition() \
			((USB_INPORT( BOOTLOADER_JUMPER_PORT) & (1<<BOOTLOADER_JUMPER_BIT)) == 0)
	
	#define bootLoaderExit() \
			{ USB_OUTPORT(BOOTLOADER_JUMPER_PORT) &= ~(1<<BOOTLOADER_JUMPER_BIT); }
#endif

#ifndef bootLoaderCondition
	static void bootLoaderInit( void ) { }
	#define bootLoaderCondition() 1
	static void bootLoaderExit( void ) { }
#endif

#if !LED_PRESENT
	#define LED_INIT()		
	#define LED_EXIT()		
	#define LED_BLINK()		
#elif !defined(LED_BLINK)
	#define LED_INIT()		{ }
	#define LED_EXIT()		{ USB_DDRPORT(LED_PORT) &= ~(1<<LED_BIT); }
	#define LED_BLINK()		{ USB_DDRPORT(LED_PORT) ^=  (1<<LED_BIT); }
#endif

//**** Defaults/constraints

#ifndef SIGNATURE_BYTES
	#define SIGNATURE_BYTES SIGNATURE_0, SIGNATURE_1, SIGNATURE_2, 0
#endif

#ifndef BOOTLOADER_CAN_EXIT
	#define BOOTLOADER_CAN_EXIT 1
#endif

#ifndef HAVE_FLASH_PAGED_READ
	#define HAVE_FLASH_PAGED_READ 1
#endif

#ifndef HAVE_EEPROM_PAGED_ACCESS
	#define HAVE_EEPROM_PAGED_ACCESS 1
#endif

#ifndef USE_GLOBAL_REGS
	#define USE_GLOBAL_REGS 1
#endif

#ifndef __GNUC__
	#undef USE_GLOBAL_REGS
#endif

#if AUTO_EXIT_MS
	#ifndef AUTO_EXIT_NO_USB
		#define AUTO_EXIT_NO_USB 1
	#endif
#endif

#if AUTO_EXIT_NO_USB
	#define AUTO_EXIT_NO_USB_MS 500
#endif

#if !BOOTLOADER_CAN_EXIT
	#undef AUTO_EXIT_NO_USB
#endif

// Compact USBASP-compatible bootloader for AVR

// License: GNU GPL v2 (see License.txt)
// Copyright (c) 2007 Christian Starkjohann
// Copyright (c) 2007 OBJECTIVE DEVELOPMENT Software GmbH
// Copyright (c) 2012 Stephan Baerwolf
// Copyright (c) 2013 Shay Green

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#ifndef MCUCSR
	#define MCUCSR MCUSR
#endif

static void leaveBootloader( void ) __attribute__((noreturn));

#include "usbconfig.h" // includes "bootloaderconfig.h" indirectly
#include "postconfig.h"
#include "usbdrv/usbdrv.h"
#include "usbdrv/oddebug.h"

#define USBASP_FUNC_CONNECT         1
#define USBASP_FUNC_DISCONNECT      2
#define USBASP_FUNC_TRANSMIT        3
#define USBASP_FUNC_READFLASH       4
#define USBASP_FUNC_ENABLEPROG      5
#define USBASP_FUNC_WRITEFLASH      6
#define USBASP_FUNC_READEEPROM      7
#define USBASP_FUNC_WRITEEEPROM     8
#define USBASP_FUNC_SETLONGADDRESS  9
#define USBASP_FUNC_SETISPSCK      10

#define CLI_SEI( expr ) do { cli(); (expr); sei(); } while ( 0 )

#if FLASHEND > 0xFFFF // >64KB flash
	typedef uint32_t addr_t;
	#define PGM_READ_BYTE pgm_read_byte_far
#else 
	typedef uint16_t addr_t;
	#define PGM_READ_BYTE pgm_read_byte
#endif

union currentAddress_t {
	addr_t   a; // full address
	uint16_t w [sizeof (addr_t) / 2]; // lower/upper words
};

#if USE_GLOBAL_REGS
	#define GLOBAL_REG( r, type, name, init ) \
		enum { name##_init = init };\
		register type name asm(#r)
#else
	#define GLOBAL_REG( r, type, name, init ) static type name = init
#endif

// Effective number of clocks per iteration of main loop. Empirically timed.
// Accounts for USB interrupts. Oddly same regardless of GLOBAL_REGS.
enum { main_loop_clk = 28 };

static union currentAddress_t currentAddress; // in bytes
GLOBAL_REG( r3, uchar, bytesRemaining, 0 );
GLOBAL_REG( r4, uchar, isLastPage, 0 ); // needs to be masked with 0x02
#if AUTO_EXIT_NO_USB_MS
	GLOBAL_REG( r5, uchar, currentRequest, USBASP_FUNC_DISCONNECT );
	GLOBAL_REG( r6, uchar, timeoutHigh,
			F_CPU/main_loop_clk / 1000 * (AUTO_EXIT_NO_USB_MS) / 0x10000 );
#else
	GLOBAL_REG( r5, uchar, currentRequest, 0 );
#endif

static uchar notErased = 1;

// **** Commands

static uchar usbFunctionSetup_USBASP_FUNC_TRANSMIT( const usbRequest_t* rq )
{
	usbWord_t u;
	u.bytes [1] = rq->wValue.bytes [1]; // big-endian
	u.bytes [0] = rq->wIndex.bytes [0];
	
	#define RQ_BYTE  (rq->wValue.bytes [0])
	#define RQ_BYTE2 (rq->wValue.bytes [1])
	
	if ( RQ_BYTE == 0x30 )
	{
		static const uchar signatureBytes [4] = { SIGNATURE_BYTES };
		uchar i = rq->wIndex.bytes [0] & 3; // optimization: separate calc
		return signatureBytes [i];
	}

#if !defined (HAVE_READ_LOCK_FUSE) || HAVE_READ_LOCK_FUSE
	else if ( RQ_BYTE == 0x50 || RQ_BYTE == 0x58 )
	{
		uchar n = 0;
		if ( RQ_BYTE & 0x08 )
			n |= 1;
		if ( RQ_BYTE2 & 0x08 )
			n |= 2;
		return boot_lock_fuse_bits_get( n ); // shows up as LPM in disassembly
	}
#endif

#if !defined (HAVE_FLASH_BYTE_READACCESS) || HAVE_FLASH_BYTE_READACCESS
	else if ( RQ_BYTE == 0x20 || RQ_BYTE == 0x28 )
	{
		addr_t a = u.word;
		a <<= 1;
		if ( RQ_BYTE & 0x08 )
			a |= 1;
		return PGM_READ_BYTE( a );
	}
#endif

#if !defined (HAVE_EEPROM_BYTE_ACCESS) || HAVE_EEPROM_BYTE_ACCESS
	else if ( RQ_BYTE == 0xA0 )
	{
		return eeprom_read_byte( (void*) u.word );
	}
	else if ( RQ_BYTE == 0xC0 )
	{
		eeprom_write_byte( (void*) u.word, rq->wIndex.bytes [1] );
	}
#endif

	else if ( RQ_BYTE == 0xAC && RQ_BYTE2 == 0x80 )
	{
		#if !HAVE_CHIP_ERASE
			notErased = 0;
		#else
			addr_t addr;
			for ( addr = 0; addr < (addr_t) BOOTLOADER_ADDRESS; addr += SPM_PAGESIZE ) 
			{
				CLI_SEI( boot_page_erase( addr ) );
				boot_spm_busy_wait();
			}
		#endif
	}
	else // ignore other commands
	{
	}
	
	return 0;
}


uchar usbFunctionSetup( uchar data [8] )
{
	const usbRequest_t* rq = (const usbRequest_t*) data;
	
	#if AUTO_EXIT_NO_USB_MS
		timeoutHigh = 2; // 1 could expire immediately
	#endif
	
	static uchar replyBuffer [4];
	usbMsgPtr = (usbMsgPtr_t) replyBuffer;
	
	currentRequest = rq->bRequest;
	
	if ( rq->bRequest == USBASP_FUNC_TRANSMIT )
	{
		replyBuffer [3] = usbFunctionSetup_USBASP_FUNC_TRANSMIT( rq );
		return 4;
	}
	else if ( rq->bRequest == USBASP_FUNC_ENABLEPROG ||
			rq->bRequest == USBASP_FUNC_SETISPSCK )
	{
		// avrdude gives error if USBASP_FUNC_ENABLEPROG not handled, and
		// warning if USBASP_FUNC_SETISPSCK not handled
		//replyBuffer [0] = 0; // optimization: always zero; unnecessary to clear
		return 1;
	}
	else if ( rq->bRequest >= USBASP_FUNC_READFLASH &&
			rq->bRequest <= USBASP_FUNC_SETLONGADDRESS )
	{
		currentAddress.w [0] = rq->wValue.word;
		if ( rq->bRequest == USBASP_FUNC_SETLONGADDRESS )
		{
			#if FLASHEND > 0xFFFF
				currentAddress.w[1] = rq->wIndex.word;
			#endif
		}
		else // USBASP_FUNC_(READ/WRITE)FLASH, USBASP_FUNC_(READ/WRITE)EEPROM
		{
			bytesRemaining = rq->wLength.bytes [0];
			isLastPage = rq->wIndex.bytes [1];
			return USB_NO_MSG; // causes callbacks to read/write functions below
		}
	}
	else // ignored: USBASP_FUNC_CONNECT, USBASP_FUNC_DISCONNECT
	{
	}
	
	return 0;
}


// **** Data read/write

uchar usbFunctionWrite( uchar* data, uchar len )
{
	if ( len > bytesRemaining )
		len = bytesRemaining;
	bytesRemaining -= len;
	uchar isLast = (bytesRemaining == 0);
	
	for ( len++; len > 1; )
	{
	#if HAVE_EEPROM_PAGED_ACCESS
		if ( currentRequest >= USBASP_FUNC_READEEPROM )
		{
			eeprom_write_byte( (void*) (currentAddress.w [0]++), *data++ );
			len--;
		}
		else
	#endif
		if ( currentAddress.a >= (addr_t) BOOTLOADER_ADDRESS )
		{
			return 1;
		}
		else
		{
			CLI_SEI( boot_page_fill( currentAddress.a, *(uint16_t*) data ) );
			
			data += 2;
			len  -= 2;
			currentAddress.a += 2;
			
			// write page after last word has been written for that page, either
			// because it was last word of page, or last one host will be writing
			if ( (currentAddress.w [0] & (SPM_PAGESIZE - 1)) == 0 ||
					(isLast && len <= 1 && isLastPage & 0x02) )
			{
				#if !HAVE_CHIP_ERASE
					if ( !notErased )
					{
						CLI_SEI( boot_page_erase( currentAddress.a - 2 ) );
						boot_spm_busy_wait();
					}
				#endif
				
				CLI_SEI( boot_page_write( currentAddress.a - 2 ) );
				boot_spm_busy_wait();
				CLI_SEI( boot_rww_enable() );
			}
			
		}
	}
	return isLast;
}

uchar usbFunctionRead( uchar* data, uchar len )
{
#if HAVE_FLASH_PAGED_READ || HAVE_EEPROM_PAGED_ACCESS
	if ( len > bytesRemaining )
		len = bytesRemaining;
	bytesRemaining -= len;
	
	addr_t a = currentAddress.a; // optimization
	uchar n;
	for ( n = len; n; n-- )
	{
		// optimization: read unconditionally, since extra pgm read is harmless
		uchar b = PGM_READ_BYTE( a );
		#if HAVE_EEPROM_PAGED_ACCESS
			#if HAVE_FLASH_PAGED_READ
				if ( currentRequest >= USBASP_FUNC_READEEPROM )
			#endif
					b = eeprom_read_byte( (void*) (uint16_t) a );
		#endif
		*data++ = b;
		a++;
	}
	currentAddress.a = a;
	
	return len;
#else
	return 0;
#endif
}


// **** Self-update

#if !defined (HAVE_SELF_UPDATE) || HAVE_SELF_UPDATE
	#include "do_spm.h"
	
	#ifdef SPM_RDY_vect
		ISR(SPM_RDY_vect,ISR_NAKED)
		{
			do_spm();
		}
	#else
		#warning "Self-update disabled; device lacks SPM_RDY_vect"
	#endif
#endif


// **** Main program

static void leaveBootloader( void )
{
	LED_EXIT();
	cli();
	usbDeviceDisconnect();
	
	USB_INTR_ENABLE = 0;
	USB_INTR_CFG    = 0; // also reset config bits
	
	SET_IVSEL( 0 );
	
	bootLoaderExit();
	
	// GCC doesn't set EIND when generating EICALL on devices with large flash.
	#ifdef EIND
		EIND = 0;
	#endif
	
	// GCC bug: optimizes this to RCALL 0, which is mishandled by assembler.
	// TODO: or not? seems to work now
	//void (*nullVector)(void) __attribute__((noreturn)) = 0;
	static void (* const nullVector)(void) __attribute__((noreturn)) = 0;
	nullVector();
}

#ifndef WDTCSR
	#define WDTCSR WDTCR
#endif

#ifndef WDTOE
	#define WDTOE WDCE
#endif

static void initHardware( void )
{
	// Clear cause-of-reset flags and try to disable WDT
	MCUCSR = 0; // WDRF must be clear or WDT can't be disabled on some MCUs
	WDTCSR = 1<<WDTOE | 1<<WDE;
	WDTCSR = 1<<WDP2 | 1<<WDP1 | 1<<WDP0; // maximum timeout in case WDT is fused on
	SET_IVSEL( 1 );
	
	usbInit();
	
	// Force USB re-enumerate so host sees us
	usbDeviceDisconnect();
	_delay_ms( 260 );
	usbDeviceConnect();
	
	sei();
	LED_INIT();
}

int main( void ) __attribute__((noreturn,OS_main)); // optimization
int main( void )
{
	#if USE_GLOBAL_REGS
		currentRequest = currentRequest_init;
		#if AUTO_EXIT_NO_USB_MS
			timeoutHigh = timeoutHigh_init;
		#endif
	#endif
	
	odDebugInit();
	
	// Allow user to see registers before any disruption
	bootLoaderInit();
	
	initHardware(); // gives time for jumper pull-ups to stabilize
	
	uchar i = 0; // tried unsigned int counter but added 60 bytes
	uchar j = 0;
	while ( bootLoaderCondition() )
	{
		wdt_reset(); // in case wdt is fused on
		usbPoll();
		
		if ( --i == 0 )
		{
			if ( --j == 0 )
			{
				LED_BLINK();
				
				#if BOOTLOADER_CAN_EXIT
					if ( currentRequest == USBASP_FUNC_DISCONNECT )
					{
						#if AUTO_EXIT_NO_USB_MS
							if ( --timeoutHigh == 0 )
						#endif
								break;
					}
				#endif
			}
		}
			
	}
	
	leaveBootloader();
}

// Called when reset is received from host, possibly multiple times
#if AUTO_EXIT_NO_USB_MS
	// TODO: less hacky approach (USB_RESET_HOOK took 18 bytes more code)
	#undef DBG1
	#if AUTO_EXIT_MS
		#define DBG1( a, b, c ) {\
			if ( (a) == 0xff )\
				timeoutHigh = F_CPU/main_loop_clk / 1000 * (AUTO_EXIT_MS) / 0x10000;\
		}
	#else
		#define DBG1( a, b, c ) {\
			if ( (a) == 0xff )\
				currentRequest = 0;\
		}
	#endif
#endif

// at end so we don't mistakenly use some of its internal variables
#include "usbdrv/usbdrv.c" // optimization: helps to have source in same file

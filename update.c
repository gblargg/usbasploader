// Updates bootloader by flashing self-contained copy, using do_spm routine

#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

static PROGMEM const uint8_t loader [] = {
	#include "obj/loader.h"
};

#define loader_end (loader + sizeof loader)

#if FLASHEND > 0xFFFF // >64KB flash
	typedef uint32_t addr_t;
	#define PGM_READ_BYTE pgm_read_byte_far
#else 
	typedef uint16_t addr_t;
	#define PGM_READ_BYTE pgm_read_byte
#endif

// Second do_spm core to use when updating bulk of bootloader
#include "do_spm.h"

// Calls do_spm routine, loading Z with addr, r1:r0 with data, and writing cmd to SPMCR
static __attribute((naked)) void call_do_spm( uint8_t cmd, uint16_t addr,
		uint16_t data, addr_t do_spm )
{
	// Pushes do_spm on stack to call, rather than using EICALL, so that Z can be used
	// for address passed to routine.
	asm volatile (
	"\n		rcall 1f"		// need to clear r1 after do_spm
	"\n		clr r1"
	#ifdef RAMPZ
	"\n		sts %[rampz], r1"
	#endif
	"\n		ret"
	"\n	1:	push r18"		// stack=do_spm
	"\n		push r19"
	#if defined (EIND) || defined (__AVR_3_BYTE_PC__)
	"\n		ldi r18, 0xff"	// RET uses 3 bytes on larger devices
	"\n		push r18"
	#endif
	#ifdef RAMPZ
	"\n		ldi r18, 0xff"
	"\n		sts %[rampz], r18"
	#endif
	"\n		movw r30, r22"	// Z=addr
	"\n		movw r0, r20"	// r1:r0=data
	"\n		ldi r26, lo8(%[spmcr])"	// X=spmcr
	"\n		ldi r27, hi8(%[spmcr])"
	"\n		ret"			// Call do_spm
	"\n" ::
	[spmcr] "M" (&SPMCR)
	#ifdef RAMPZ
	,[rampz] "M" (&RAMPZ)
	#endif
	);
}

// Copy page from in to addr in flash
static void copy_page( addr_t addr, const uint8_t in [], const uint8_t* in_end, addr_t do_spm )
{
	// Erase flash page
	call_do_spm( 1<<PGERS | 1<<SPMEN, addr, 0, do_spm );
	
	int n;
	for ( n = SPM_PAGESIZE/2; n; n-- )
	{
		// Pad last page of loader to page size with 0xff
		unsigned data = 0xffff;
		if ( in < in_end )
		{
			data = pgm_read_word( in );
			in += 2;
		}
		
		// Write word into page buffer
		call_do_spm( 1<<SPMEN, addr, data, do_spm );
		addr += 2;
	}
	
	// Write buffer to flash page
	call_do_spm( 1<<PGWRT | 1<<SPMEN, addr - 2, 0, do_spm );
}

// Only way to get SPM_RDY_vect vector number below
#undef _VECTOR
#define _VECTOR(n) n

// Updates bootloader
static void update_loader( void )
{
	// Addresses
	const addr_t do_spm1_addr = SPM_RDY_vect*2 + BOOTLOADER_ADDRESS;
	const addr_t do_spm2_addr = FLASHEND - SPM_PAGESIZE + 1;
	const addr_t do_spm1 = do_spm1_addr/2;
	const addr_t do_spm2 = do_spm2_addr/2;
	
	const uint8_t* const do_spm2_in = (uint8_t*) ((unsigned) do_spm*2);
	
	// Put do_spm2 at top of flash
	copy_page( do_spm2_addr, do_spm2_in, do_spm2_in + SPM_PAGESIZE, do_spm1 );
	
	// Copy pages, using do_spm2 for all except last (if the loader is even that big)
	addr_t addr = BOOTLOADER_ADDRESS;
	const uint8_t* in;
	for ( in = loader; in < loader_end; in += SPM_PAGESIZE )
	{
		copy_page( addr, in, loader_end, (addr < do_spm2_addr ? do_spm2 : do_spm1) );
		addr += SPM_PAGESIZE;
	}
}

// True if loader[] matches current bootloader
static bool needs_update( void )
{
	// Prevent optimizer from assuming that is_updated() returns the same
	// value every time it's called (it assumes pgm_read is pure)
	static volatile addr_t bl_addr = BOOTLOADER_ADDRESS;
	
	const uint8_t* in = loader;
	addr_t old = bl_addr;
	do
	{
		if ( PGM_READ_BYTE( in ) != PGM_READ_BYTE( old ) )
			return true;
		in++;
		old++;
	}
	while ( in < loader_end );
	return false;
}

int main( void )
{
	if ( needs_update() )
	{
		update_loader();
		
		// Hang if update failed
		while ( needs_update() )
			{ }
	}
	
	// Watchdog reset into new bootloader
	wdt_enable( WDTO_15MS );
	for ( ;; )
		{ }
}

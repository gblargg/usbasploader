// SPM instruction inside a routine that can be put in bootloader and called from user program.
// Necessary because SPM can only be executed when instruction's address is in bootloader region.

#if !defined (SPM_RDY_vect) && defined (SPM_READY_vect)
	#define SPM_RDY_vect SPM_READY_vect
#endif

#ifndef SPMCR
	#define SPMCR SPMCSR
#endif

#ifdef SPM_RDY_vect

// Main goal is minimal size. We take address of SPMCR in X to save a few bytes.
// This also makes code less-likely to accidentally write to flash, since X being
// anything other than SPMCR prevents it from affecting flash.

static __attribute__((naked)) void do_spm( void )
{
	asm volatile ( // On entry, X=SPMCR, r24=command, Z=address, r1:r0=data
	"\n	1:	st X, r24"			// Do SPM operation in r24
	"\n		spm"
	"\n	2:	ld r25, X"			// Wait until operation finishes
	"\n		sbrc r25, %[spmen]"
	"\n		rjmp 2b"
	"\n		ldi r24, %[spmret]"	// Prepare for RWWSRE
	"\n		sbrc r25, %[rwwsb]"	// Set RWSSRE if RWWSB is still set
	"\n		rjmp 1b"
	"\n		ret"
	"\n" ::
	[spmen]  "I" (SPMEN),
	[rwwsb]  "I" (RWWSB),
	[spmret] "M" (1<<RWWSRE | 1<<SPMEN)
	);
}

#endif

! -*- Fundamental -*-
! This is the file Sparc/memory.s.
!
! Larceny run-time system (SPARC)  --  memory management primitives.
!
! History:
!   June 24 - July 1, 1994 / lth
!     _Major_ rewrite and clean-up. Internal conventions were cleaned
!     up, a number of entry points were removed, and the rest were
!     made comprehensible. Old lies were refuted and new thruths were
!     postulated. Massive improvement in documentation and calling
!     conventions.
!
!   Sometime in August 1992 / lth
!     Part of version 0.14.
!
!   Sometime during spring of 1991 / lth
!     Created.
!
! Naming conventions:
!   All publicly available procedures are named _mem_something.
!   Procedures which use internal calling conventions (i.e., which are
!   called from millicode) are named _mem_internal_something.
!
! Calling conventions:
!   Scheme-to-millicode: return address in %o7, valid %R0, arguments
!   in RESULT, ARGREG2, ARGREG3. Always in Scheme mode.
!
!   Millicode-to-millicode: return address in %o7, Scheme return
!   address saved in G_RETADDR, %R0 valid, arguments in RESULT,
!   ARGREG2, ARGREG3.
!
! Assumptions:
!   - all allocation is in 8-byte aligned chunks
!   - the ephemeral space lives below the tenured space

#include "asmdefs.h"

	.global _mem_alloc				! allocate raw RAM
	.global _mem_alloci				! allocate cooked RAM
	.global	_mem_internal_alloc			! allocate raw RAM
	.global _mem_garbage_collect			! do a GC
	.global _mem_internal_collect			! do a GC
	.global _mem_addtrans				! remember object
	.global _mem_stkoflow				! handle stack oflow
	.global	_mem_internal_stkoflow			! handle stack oflow
	.global _mem_stkuflow				! handle stack uflow
	.global _mem_capture_continuation		! creg-get
	.global _mem_restore_continuation		! creg-set!

	.seg "text"


! _mem_alloc: allocate uninitialized memory.
!
! Call from: Scheme
! Input    : RESULT = fixnum: size of structure in words
!            o7 = scheme return address
! Output   : RESULT = untagged ptr to uninitialized memory
! Destroys : RESULT, Temporaries
!
! This procedure could call _mem_internal_alloc, but does its work
! in-line for performance reasons.

_mem_alloc:
	add	%E_TOP, %RESULT, %E_TOP		! allocate optimistically
	and	%RESULT, 0x04, %TMP1		! get 'odd' bit
	cmp	%E_TOP, %E_LIMIT		! check for overflow
	blt,a	Lalloc1				! skip of no overflow
	sub	%E_TOP, %RESULT, %RESULT	! setup result

	! Heap overflow. %RESULT still has the number of words to alloc.

	st	%o7, [ %GLOBALS + G_RETADDR ]	! save scheme return address
	call	heap_overflow			! deal with overflow
	sub	%E_TOP, %RESULT, %E_TOP		! recover
	b	_mem_alloc			! redo operation
	ld	[ %GLOBALS + G_RETADDR ], %o7	! restore return address

Lalloc1:
	jmp	%o7+8
	add	%E_TOP, %TMP1, %E_TOP		! Round up. We can round
						! without checking for
						! overflow because everything
						! is 8-byte aligned.


! _mem_alloci: allocate initialized memory
!
! Call from: Scheme
! Input    : RESULT = fixnum: size of structure, in words
!            ARGREG2 = object: to initialize with
!            o7 = Scheme return address
! Output   : RESULT = untagged ptr to uninitialized structure
! Destroys : RESULT, Temporaries

_mem_alloci:
	st	%o7, [ %GLOBALS + G_RETADDR ]	! save Scheme retaddr
	st	%ARGREG3, [ %GLOBALS + G_ALLOCI_TMP ]

	call	_mem_internal_alloc		! allocate memory
	mov	%RESULT, %ARGREG3		! save size for later
	ld	[ %GLOBALS + G_RETADDR ], %o7	! restore Scheme retaddr

	! %RESULT now has ptr, %ARGREG3 has count, %ARGREG2 has obj

	sub	%RESULT, 0x04, %TMP1		! dest = RESULT - 4
	b	Lalloci2
	tst	%ARGREG3
Lalloci3:
	st	%ARGREG2, [ %TMP1 ]		! init a word
	deccc	4, %ARGREG3			! n -= 4, test n
Lalloci2:
	bne	Lalloci3
	add	%TMP1, 0x04, %TMP1		! dest += 4
	jmp	%o7+8
	ld	[ %GLOBALS + G_ALLOCI_TMP ], %ARGREG3


! _mem_internal_alloc: allocate uninitialized memory.
!
! Call from: Millicode
! Input    : RESULT = fixnum: size of structure in words
!            o7 = millicode return address
! Output   : RESULT = untagged ptr to uninitialized memory
! Destroys : RESULT, Temporaries

_mem_internal_alloc:
	add	%E_TOP, %RESULT, %E_TOP		! allocate optimistically
	and	%RESULT, 0x04, %TMP1		! get 'odd' bit
	cmp	%E_TOP, %E_LIMIT		! check for overflow
	blt,a	Lialloc1			! skip of no overflow
	sub	%E_TOP, %RESULT, %RESULT	! setup result

	! Heap overflow.

	mov	%o7, %TMP0			! save
	call	internal_push			!   retaddr
	nop

	call	heap_overflow			! deal with overflow
	sub	%E_TOP, %RESULT, %E_TOP		! restore heap ptr

	call	internal_pop			! restore retaddr
	nop
	b	_mem_internal_alloc		! do it again
	mov	%TMP0, %o7

Lialloc1:
	jmp	%o7+8
	add	%E_TOP, %TMP1, %E_TOP		! round up; see justification
						! in code for _mem_alloc.


! heap_overflow: Heap overflow handler for allocation primitives.
! ETOP must point to first free word in ephemeral area.
!
! Call from: allocation millicode procedures only
! Input    : RESULT = fixnum: size of structure to allocate, in words
! Output   : Nothing
! Destroys : Temporaries

heap_overflow:
	set	EPHEMERAL_COLLECTION, %TMP1	! type of collection
	mov	%RESULT, %TMP2			! words requested
	set	_C_garbage_collect, %TMP0
	b	internal_callout_to_C
	nop


! _mem_garbage_collect: perform a garbage collection
!
! Call from: Scheme
! Input    : RESULT = fixnum: type of collection.
! Output   : Nothing
! Destroys : Temporaries

_mem_garbage_collect:
	st	%o7, [ %GLOBALS + G_RETADDR ]
	call	_mem_internal_collect
	nop
	ld	[ %GLOBALS + G_RETADDR ], %o7
	jmp	%o7+8
	nop


! _mem_internal_collect: perform a garbage collection, using internal
! calling conventions.
!
! Call from: millicode only
! Input    : RESULT = fixnum: collection type
! Output   : nothing
! Destroys : Temporaries

_mem_internal_collect:
	mov	%RESULT, %TMP1			! gc type
	set	0, %TMP2			! words requested
	set	_C_garbage_collect, %TMP0
	b	internal_callout_to_C
	nop
	

! _mem_addtrans: create transaction if necessary
!
! Call from: Scheme
! Input:     RESULT = object: object which was assigned to.
!            ARGREG2 = object: object which was assigned.
! Output:    Nothing
! Destroys:  Temporaries
!
! [NOTE: The following does not explain the experimental collector.]
!
! To check that the assigned-to object is indeed ephemeral we must
! compare to the real upper limit of the ephemeral space, not to E_TOP
! or E_LIM. The reason for this is that the latter provide only a
! conservative approximation; if the continuation is captured by a
! program and then manipulated destructively (e.g. with a debugger) before
! a GC, then the assignment to an ephemeral object will be seen here
! as an assignment to a tenured object if the conservative limit is used.
! 
! In-line code may validly use the conservative limits.

_mem_addtrans:
#if 0
	! This code is for the experimental collector (exgc).
	ld	[ %GLOBALS + G_TBRK ], %TMP0	! limit of espace
	cmp	%RESULT, %TMP0			! ephemeral RHS?
	bge	Laddtrans_exit			! exit if so
	nop
#endif
	ld	[ %GLOBALS + G_ELIM ], %TMP0	! limit of cspace
	cmp	%RESULT, %TMP0			! ephemeral LHS?
	blt	Laddtrans_exit			! exit if so
	nop
	andcc	%ARGREG2, 0x01, %g0		! constant RHS?
	be	Laddtrans_exit			! exit if so
	nop
	cmp	%ARGREG2, %TMP0			! ephemeral RHS?
	bge	Laddtrans_exit			! exit if not
	nop

	! At this point we must add a transaction to the SSB.

	ld	[ %GLOBALS + G_SSBTOP ], %TMP0	! get top
	st	%RESULT, [ %TMP0 ]		! store trans
	inc	4, %TMP0			! bump top
	ld	[ %GLOBALS+G_SSBLIM ], %TMP1	! get highest
	st	%TMP0, [ %GLOBALS + G_SSBTOP ]	! store top
	cmp	%TMP0, %TMP1			! at end?
	bne	Laddtrans_exit			! done if not
	nop

	! Filled the SSB; deal with it on a higher level, return to Scheme.

	set	_C_compact_ssb, %TMP0
	b	callout_to_C
	nop
	! never returns

Laddtrans_exit:
	jmp	%o7+8
	nop


! _mem_stkuflow: stack underflow handler.
!
! Call from: Don't, it should be returned to.
! Input    : Nothing
! Output   : Nothing
! Destroys : Temporaries
!
! This is designed to be returned through on a stack cache underflow.
! It _can_ be called from scheme, and is, in the current implementation,
! due to the compiler bug with spill frames.

_mem_stkuflow:
	! This is where it starts when called
	b	Lstkuflow1
	nop
	! This is where it starts when returned into; must be 8 bytes from
	! the label _mem_stkuflow.
#if 1
	! The code in the #if ... #endif is a transcription of
	! the code in the C procedure restore_frame() in stack.c.
	! By moving it in-line we save two context switches, a very
	! significant part of the cost since it is incurred on
	! every underflow. On deeply recursive code (like append-rec)
	! this fix pays off with a speedup of 15-50%
	!
	! If you change this code, be sure to check the C code as well!
	! The code is in an #if ... #endif so that it can be turned off
	! to measure gains or look for bugs.

	ld	[ %GLOBALS + G_CONT ], %TMP0	! get heap frame ptr
	ld	[ %TMP0 - VEC_TAG ], %TMP1	! get header
	srl	%TMP1, 10, %TMP1		! size in words
	inc	%TMP1				! need to copy header too
	! now rounding up to even words
	andcc	%TMP1, 1, %g0
	bne,a	.+8
	inc	%TMP1
	! Allocate frame, check for overflow
	sll	%TMP1, 2, %TMP2			! must subtract bytes...
	sub	%STKP, %TMP2, %STKP
	cmp	%STKP, %E_TOP
	bl,a	1f
	add	%STKP, %TMP2, %STKP
	! Need a temp
	st	%RESULT, [ %GLOBALS + G_RESULT ]
	! While more frames to copy...
	!  TMP1 has loop count (even # of words),
	!  TMP0 has src (heap frame),
	!  TMP2 has dest (stack pointer).
	mov	%STKP, %TMP2
	dec	VEC_TAG, %TMP0
	b	2f
	tst	%TMP1
3:
	inc	4, %TMP0
	st	%RESULT, [ %TMP2 ]
	inc	4, %TMP2
	ld	[ %TMP0 ], %RESULT
	inc	4, %TMP0
	st	%RESULT, [ %TMP2 ]
	inc	4, %TMP2
	deccc	2, %TMP1
2:
	bne,a	3b
	ld	[ %TMP0 ], %RESULT
	! Restore that temp
	ld	[ %GLOBALS + G_RESULT ], %RESULT
	! follow continuation chain
	ld	[ %STKP + 8 ], %TMP0
	st	%TMP0, [ %GLOBALS + G_CONT ]
	! convert size field in frame
	ld	[ %STKP ], %TMP0
	sra	%TMP0, 8, %TMP0
	st	%TMP0, [ %STKP ]
	! convert return address
	ld	[ %STKP+4 ], %TMP0		! return offset
	call	internal_fixnum2retaddr
	ld	[ %STKP+12 ], %REG0		! procedure
	jmp	%TMP0+8
	st	%TMP0, [ %STKP+4 ]		! to be on the safe side

	! If we get to this point, the heap overflowed, so just call
	! the C version and let it deal with it.
1:
#endif
	set	_C_restore_frame, %TMP0
	mov	0, %REG0			! procedure no longer valid...
	call	internal_callout_to_C
	nop
	ld	[ %STKP+4 ], %o7
	jmp	%o7+8
	nop
	! This code goes away when the compiler is fixed.
Lstkuflow1:
	set	_C_restore_frame, %TMP0
	st	%o7, [ %GLOBALS + G_RETADDR ]
	call	internal_callout_to_C
	nop
	ld	[ %GLOBALS + G_RETADDR ], %o7
	jmp	%o7+8
	nop

! _mem_stkoflow: stack overflow handler
!
! Call from: Scheme
! Input    : Nothing
! Output   : Nothing
! Destroys : Temporaries

_mem_stkoflow:
	set	_C_stack_overflow, %TMP0
	b	callout_to_C
	nop


! _mem_internal_stkoflow: millicode-internal stack overflow handler.
!
! Call from: Millicode
! Input    : Nothing
! Output   : Nothing
! Destroys : Temporaries

_mem_internal_stkoflow:
	set	_C_stack_overflow, %TMP0
	b	internal_callout_to_C
	nop


! _mem_capture_continuation: perform a creg-get
!
! Call from: Scheme
! Input    : Nothing
! Output   : RESULT = obj: continuation object
! Destroys : Temporaries, RESULT

_mem_capture_continuation:
	set	_C_creg_get, %TMP0
	b	callout_to_C
	nop


! _mem_restore_continuation: perform a creg-set!
!
! Call from: Scheme
! Input    : RESULT = obj: continuation object
! Output   : Nothing
! Destroys : Temporaries

_mem_restore_continuation:
	set	_C_creg_set, %TMP0
	b	callout_to_C
	nop


	! end of file

/*	$NetBSD: locore.s,v 1.28.2.2 1997/11/07 22:16:32 mellon Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: locore.s 1.66 92/12/22$
 *
 *	@(#)locore.s	8.6 (Berkeley) 5/27/94
 */

#include "assym.h"
#include <machine/asm.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

/*
 * Temporary stack for a variety of purposes.
 * Try and make this the first thing is the data segment so it
 * is page aligned.  Note that if we overflow here, we run into
 * our text segment.
 */
	.data
	.space	NBPG
tmpstk:

#define	RELOC(var, ar) \
	lea	var,ar

#define	CALLBUG(func)	\
	trap #15; .short func

/*
 * Initialization
 *
 * The bootstrap loader loads us in starting at 0, and VBR is non-zero.
 * On entry, args on stack are boot device, boot filename, console unit,
 * boot flags (howto), boot device name, filesystem type name.
 */
	.comm	_lowram,4
	.comm	_esym,4

	.text
	.globl	_edata
	.globl	_etext,_end
	.globl	start,_kernel_text
| This is for kvm_mkdb, and should be the address of the beginning
| of the kernel text segment (not necessarily the same as kernbase).
_kernel_text:
start:					| start of kernel and .text!
	movw	#PSL_HIGHIPL,sr		| no interrupts
	movl	#0,a5			| RAM starts at 0 (a5)
	movl	sp@(4), d7		| get boothowto
	movl	sp@(8), d6		| get bootaddr
	movl	sp@(12),d5		| get bootctrllun
	movl	sp@(16),d4		| get bootdevlun
	movl	sp@(20),d3		| get bootpart
	movl	sp@(24),d2		| get esyms

	RELOC(_bootpart,a0)
	movl	d3, a0@			| save bootpart
	RELOC(_bootdevlun,a0)
	movl	d4, a0@			| save bootdevlun
	RELOC(_bootctrllun,a0)
	movl	d5, a0@			| save booctrllun
	RELOC(_bootaddr,a0)
	movl	d6, a0@			| save bootaddr
	RELOC(_boothowto,a0)
	movl	d7, a0@			| save boothowto
	/* note: d3-d7 free, d2 still in use */

	RELOC(tmpstk, a0)
	movl	a0,sp			| give ourselves a temporary stack

	RELOC(_edata,a0)		| clear out BSS
	movl	#_end-4,d0		| (must be <= 256 kB)
	subl	#_edata,d0
	lsrl	#2,d0
1:	clrl	a0@+
	dbra	d0,1b

	RELOC(_esym, a0)
	movl	d2,a0@			| store end of symbol table
	/* d2 now free */
	RELOC(_lowram, a0)
	movl	a5,a0@			| store start of physical memory

	movl	#CACHE_OFF,d0
	movc	d0,cacr			| clear and disable on-chip cache(s)

	/* ask the Bug what we are... */
	clrl	sp@-
	CALLBUG(MVMEPROM_GETBRDID)
	movl	sp@+,a1

	/* copy to a struct mvmeprom_brdid */
	movl	#MVMEPROM_BRDID_SIZE,d0
	RELOC(_boardid,a0)
1:	movb	a1@+,a0@+
	subql	#1,d0
	bne	1b

	/*
	 * Grab the model number from _boardid and use the value
	 * to setup machineid, cputype, and mmutype.
	 */
	clrl	d0
	RELOC(_boardid,a1)
	movw	a1@(MVMEPROM_BRDID_MODEL_OFFSET),d0
	RELOC(_machineid,a0)
	movl	d0,a0@

#ifdef MVME147
	/* MVME-147 - 68030 CPU/MMU, 68882 FPU */
	cmpw	#MVME_147,d0
	jne	Lnot147
	RELOC(_mmutype,a0)
	movl	#MMU_68030,a0@
	RELOC(_cputype,a0)
	movl	#CPU_68030,a0@
	RELOC(_fputype,a0)
	movl	#FPU_68882,a0@

	/* XXXCDC SHUTUP 147 CALL */
	movb	#0, 0xfffe1026		| serial interrupt off
	movb	#0, 0xfffe1018		| timer 1 off
	movb	#0, 0xfffe1028		| ethernet off
	/* XXXCDC SHUTUP 147 CALL */

	/* Save our ethernet address */
	RELOC(_myea, a0)
	movl	0xfffe0778,a0@		| XXXCDC -- HARDWIRED HEX

	/* initialize memory sizes (for pmap_bootstrap) */
#ifndef MACHINE_NONCONTIG
	movl	0xfffe0774,d1		| XXXCDC -- hardwired HEX
	moveq	#PGSHIFT,d2
	lsrl	d2,d1			| convert to page (click) number
	RELOC(_maxmem, a0)
	movl	d1,a0@			| save as maxmem
	movl	a5,d0			| lowram value from ROM via boot
	lsrl	d2,d0			| convert to page number
	subl	d0,d1			| compute amount of RAM present
	RELOC(_physmem, a0)
	movl	d1,a0@			| and physmem
#else
	/* initialise list of physical memory segments */
	RELOC(_phys_seg_list, a0)
	movl	a5,a0@			| phys_seg_list[0].ps_start
	movl	0xfffe0774,d1		| End + 1 of onboard memory
	movl	d1,a0@(4)		| phys_seg_list[0].ps_end
	clrl	a0@(8)			| phys_seg_list[0].ps_startpage

	/* offboard RAM */
	clrl	a0@(0x0c)		| phys_seg_list[1].ps_start
	movl	#NBPG-1,d0
	addl	0xfffe0764,d0		| Start of offboard segment
	andl	#-NBPG,d0		| Round up to page boundary
	beq	Lsavmaxmem		| Jump if none defined
	movl	#NBPG,d1		| Note: implicit '+1'
	addl	0xfffe0768,d1		| End of offboard segment
	andl	#-NBPG,d1		| Round up to page boundary
	cmpl	d1,d0			| Quick and dirty validity check
	bcss	Loff_ok			| Yup, looks good.
	movel	a0@(4),d1		| Just use onboard RAM otherwise
	bras	Lsavmaxmem
Loff_ok:
	movl	d0,a0@(0x0c)		| phys_seg_list[1].ps_start
	movl	d1,a0@(0x10)		| phys_seg_list[1].ps_end
	clrl	a0@(0x14)		| phys_seg_list[1].ps_startpage

	/*
	 * Offboard RAM needs to be cleared to zero to initialise parity
	 * on most VMEbus RAM cards. Without this, some cards will buserr
	 * when first read.
	 */
	movel	d0,a0			| offboard start address again.
Lclearoff:
	clrl	a0@+			| zap a word
	cmpl	a0,d1			| reached end?
	bnes	Lclearoff

Lsavmaxmem:
	moveq	#PGSHIFT,d2
	lsrl	d2,d1			| convert to page (click) number
	RELOC(_maxmem, a0)
	movl	d1,a0@			| save as maxmem
#endif

	jra	Lstart1
Lnot147:
#endif

#ifdef MVME162
	/* MVME-162 - 68040 CPU/MMU/FPU */
	cmpw	#MVME_162,d0
	jne	Lnot162
	RELOC(_mmutype,a0)
	movl	#MMU_68040,a0@
	RELOC(_cputype,a0)
	movl	#CPU_68040,a0@
	RELOC(_fputype,a0)
	movl	#FPU_68040,a0@
#if 1	/* XXX */
	jra	Lnotyet
#else
	/* XXX more XXX */
	jra	Lstart1
#endif
Lnot162:
#endif

#ifdef MVME167
	/* MVME-167 (also 166) - 68040 CPU/MMU/FPU */
	cmpw	#MVME_166,d0
	jeq	Lis167
	cmpw	#MVME_167,d0
	jne	Lnot167
Lis167:
	RELOC(_mmutype,a0)
	movl	#MMU_68040,a0@
	RELOC(_cputype,a0)
	movl	#CPU_68040,a0@
	RELOC(_fputype,a0)
	movl	#FPU_68040,a0@
#if 1	/* XXX */
	jra	Lnotyet
#else
	/* XXX more XXX */
	jra	Lstart1
#endif
Lnot167:
#endif

#ifdef MVME177
	/* MVME-177 (what about 172??) - 68060 CPU/MMU/FPU */
	cmpw	#MVME_177,d0
	jne	Lnot177
	RELOC(_mmutype,a0)
	movl	#MMU_68060,a0@
	RELOC(_cputype,a0)
	movl	#CPU_68060,a0@
	RELOC(_fputype,a0)
	movl	#FPU_68060,a0@
#if 1
	jra	Lnotyet
#else
	/* XXX more XXX */
	jra	Lstart1
#endif
Lnot177:
#endif

	/*
	 * If we fall to here, the board is not supported.
	 * Print a warning, then drop out to the Bug.
	 */
	.data
Lnotconf:
	.ascii	"Sorry, the kernel isn't configured for this model."
Lenotconf:

	.even
	.text
	movl	#Lenotconf,sp@-
	movl	#Lnotconf,sp@-
	CALLBUG(MVMEPROM_OUTSTRCRLF)
	addql	#8,sp			| clean up stack after call

	CALLBUG(MVMEPROM_EXIT)
	/* NOTREACHED */

Lnotyet:
	/*
	 * If we get here, it means a particular model
	 * doesn't have the necessary support code in the
	 * kernel.  Print a warning, then drop out to the Bug.
	 */
	.data
Lnotsupp:
	.ascii	"Sorry, NetBSD doesn't support this model yet."
Lenotsupp:

	.even
	.text
	movl	#Lenotsupp,sp@-
	movl	#Lnotsupp,sp@-
	CALLBUG(MVMEPROM_OUTSTRCRLF)
	addql	#8,sp			| clean up stack after call

	CALLBUG(MVMEPROM_EXIT)
	/* NOTREACHED */

Lstart1:
/* initialize source/destination control registers for movs */
	moveq	#FC_USERD,d0		| user space
	movc	d0,sfc			|   as source
	movc	d0,dfc			|   and destination of transfers
/* configure kernel and proc0 VA space so we can get going */
	.globl	_Sysseg, _pmap_bootstrap, _avail_start
#ifdef DDB
	RELOC(_esym,a0)			| end of static kernel test/data/syms
	movl	a0@,d2
	jne	Lstart2
#endif
	movl	#_end,d2		| end of static kernel text/data
Lstart2:
	addl	#NBPG-1,d2
	andl	#PG_FRAME,d2		| round to a page
	movl	d2,a4
	addl	a5,a4			| convert to PA
	movl	#0, sp@-		| firstpa
	pea	a4@			| nextpa
	RELOC(_pmap_bootstrap,a0)
	jbsr	a0@			| pmap_bootstrap(firstpa, nextpa)
	addql	#8,sp

/*
 * Enable the MMU.
 * Since the kernel is mapped logical == physical, we just turn it on.
 */
	RELOC(_Sysseg, a0)		| system segment table addr
	movl	a0@,d1			| read value (a KVA)
	addl	a5,d1			| convert to PA
	RELOC(_mmutype, a0)
	cmpl	#MMU_68040,a0@		| 68040?
	jne	Lmotommu1		| no, skip
	.long	0x4e7b1807		| movc d1,srp
	jra	Lstploaddone
Lmotommu1:
	RELOC(_protorp, a0)
	movl	#0x80000202,a0@		| nolimit + share global + 4 byte PTEs
	movl	d1,a0@(4)		| + segtable address
	pmove	a0@,srp			| load the supervisor root pointer
	movl	#0x80000002,a0@		| reinit upper half for CRP loads
Lstploaddone:
	RELOC(_mmutype, a0)
	cmpl	#MMU_68040,a0@		| 68040?
	jne	Lmotommu2		| no, skip
	moveq	#0,d0			| ensure TT regs are disabled
	.long	0x4e7b0004		| movc d0,itt0
	.long	0x4e7b0005		| movc d0,itt1
	.long	0x4e7b0006		| movc d0,dtt0
	.long	0x4e7b0007		| movc d0,dtt1
	.word	0xf4d8			| cinva bc
	.word	0xf518			| pflusha
	movl	#0x8000,d0
	.long	0x4e7b0003		| movc d0,tc
	movl	#0x80008000,d0
	movc	d0,cacr			| turn on both caches
	jmp	Lenab1
Lmotommu2:
	RELOC(_prototc, a2)
	movl	#0x82c0aa00,a2@		| value to load TC with
	pmove	a2@,tc			| load it

/*
 * Should be running mapped from this point on
 */
Lenab1:
/* select the software page size now */
	lea	tmpstk,sp		| temporary stack
	jbsr	_vm_set_page_size	| select software page size
/* set kernel stack, user SP, and initial pcb */
	movl	_proc0paddr,a1		| get proc0 pcb addr
	lea	a1@(USPACE-4),sp	| set kernel stack to end of area
	lea	_proc0,a2		| initialize proc0.p_addr so that
	movl	a1,a2@(P_ADDR)		|   we don't deref NULL in trap()
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP
	movl	a1,_curpcb		| proc0 is running

	tstl	_fputype		| Have an FPU?
	jeq	Lenab2			| No, skip.
	clrl	a1@(PCB_FPCTX)		| ensure null FP context
	movl	a1,sp@-
	jbsr	_m68881_restore		| restore it (does not kill a1)
	addql	#4,sp
Lenab2:
/* flush TLB and turn on caches */
	jbsr	_TBIA			| invalidate TLB
	cmpl	#MMU_68040,_mmutype	| 68040?
	jeq	Lnocache0		| yes, cache already on
	movl	#CACHE_ON,d0
	movc	d0,cacr			| clear cache(s)
	jra	Lnocache0
Lnocache0:
/* final setup for C code */
	movl	#_vectab,d0		| set VBR
	movc	d0,vbr
	jbsr	_mvme68k_init		| additional pre-main initialization
	movw	#PSL_LOWIPL,sr		| lower SPL

/*
 * Create a fake exception frame so that cpu_fork() can copy it.
 * main() nevers returns; we exit to user mode from a forked process
 * later on.
 */
	clrw	sp@-			| vector offset/frame type
	clrl	sp@-			| PC - filled in by "execve"
	movw	#PSL_USER,sp@-		| in user mode
	clrl	sp@-			| stack adjust count and padding
	lea	sp@(-64),sp		| construct space for D0-D7/A0-A7
	lea	_proc0,a0		| save pointer to frame
	movl	sp,a0@(P_MD_REGS)	|   in proc0.p_md.md_regs

	jra	_main			| main()

	.globl _proc_trampoline
_proc_trampoline:
	movl	a3@(P_MD_REGS),sp	| process' frame pointer in sp
	movl    a3,sp@-
	jbsr    a2@
	addql   #4,sp
	movl    sp@(FR_SP),a0           | grab and load
	movl    a0,usp                  |   user SP
	moveml  sp@+,#0x7FFF            | restore most user regs
	addql   #8,sp                   | toss SP and stack adjust
	jra     rei                     | and return

/*
 * Use common m68k sigcode.
 */
#include <m68k/m68k/sigcode.s>

/*
 * Trap/interrupt vector routines
 */ 
#include <m68k/m68k/trap_subr.s>

	.text
	.globl	_trap, _nofault, _longjmp
_buserr:
	tstl	_nofault		| device probe?
	jeq	Lberr			| no, handle as usual
	movl	_nofault,sp@-		| yes,
	jbsr	_longjmp		|  longjmp(nofault)
Lberr:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	_addrerr		| no, skip
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
	moveq	#0,d0
	movw	a1@(12),d0		| grab SSW
	movl	a1@(20),d1		| and fault VA
	btst	#11,d0			| check for mis-aligned access
	jeq	Lberr2			| no, skip
	addl	#3,d1			| yes, get into next page
	andl	#PG_FRAME,d1		| and truncate
Lberr2:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	btst	#10,d0			| ATC bit set?
	jeq	Lisberr			| no, must be a real bus error
	movc	dfc,d1			| yes, get MMU fault
	movc	d0,dfc			| store faulting function code
	movl	sp@(4),a0		| get faulting address
	.word	0xf568			| ptestr a0@
	movc	d1,dfc
	.long	0x4e7a0805		| movc mmusr,d0
	movw	d0,sp@			| save (ONLY LOW 16 BITS!)
	jra	Lismerr
#endif
_addrerr:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lbenot040		| no, skip
	movl	a1@(8),sp@-		| yes, push fault address
	clrl	sp@-			| no SSW for address fault
	jra	Lisaerr			| go deal with it
Lbenot040:
#endif
	moveq	#0,d0
	movw	a1@(10),d0		| grab SSW for fault processing
	btst	#12,d0			| RB set?
	jeq	LbeX0			| no, test RC
	bset	#14,d0			| yes, must set FB
	movw	d0,a1@(10)		| for hardware too
LbeX0:
	btst	#13,d0			| RC set?
	jeq	LbeX1			| no, skip
	bset	#15,d0			| yes, must set FC
	movw	d0,a1@(10)		| for hardware too
LbeX1:
	btst	#8,d0			| data fault?
	jeq	Lbe0			| no, check for hard cases
	movl	a1@(16),d1		| fault address is as given in frame
	jra	Lbe10			| thats it
Lbe0:
	btst	#4,a1@(6)		| long (type B) stack frame?
	jne	Lbe4			| yes, go handle
	movl	a1@(2),d1		| no, can use save PC
	btst	#14,d0			| FB set?
	jeq	Lbe3			| no, try FC
	addql	#4,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe3:
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, done
	addql	#2,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe4:
	movl	a1@(36),d1		| long format, use stage B address
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, all done
	subql	#2,d1			| yes, adjust address
Lbe10:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	movw	a1@(6),d0		| get frame format/vector offset
	andw	#0x0FFF,d0		| clear out frame format
	cmpw	#12,d0			| address error vector?
	jeq	Lisaerr			| yes, go to it
	movl	d1,a0			| fault address
	movl	sp@,d0			| function code from ssw
	btst	#8,d0			| data fault?
	jne	Lbe10a
	movql	#1,d0			| user program access FC
					| (we dont seperate data/program)
	btst	#5,a1@			| supervisor mode?
	jeq	Lbe10a			| if no, done
	movql	#5,d0			| else supervisor program access
Lbe10a:
	ptestr	d0,a0@,#7		| do a table search
	pmove	psr,sp@			| save result
	movb	sp@,d1
	btst	#2,d1			| invalid (incl. limit viol. and berr)?
	jeq	Lmightnotbemerr		| no -> wp check
	btst	#7,d1			| is it MMU table berr?
	jeq	Lismerr			| no, must be fast
	jra	Lisberr1		| real bus err needs not be fast.
Lmightnotbemerr:
	btst	#3,d1			| write protect bit set?
	jeq	Lisberr1		| no: must be bus error
	movl	sp@,d0			| ssw into low word of d0
	andw	#0xc0,d0		| Write protect is set on page:
	cmpw	#0x40,d0		| was it read cycle?
	jeq	Lisberr1		| yes, was not WPE, must be bus err
Lismerr:
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lisaerr:
	movl	#T_ADDRERR,sp@-		| mark address error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lisberr1:
	clrw	sp@			| re-clear pad word
Lisberr:
	movl	#T_BUSERR,sp@-		| mark bus error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it

/*
 * FP exceptions.
 */
_fpfline:
#if defined(M68040)
	cmpw	#0x202c,sp@(6)		| format type 2?
	jne	_illinst		| no, not an FP emulation
#ifdef FPSP
	.globl	fpsp_unimp
	jmp	fpsp_unimp		| yes, go handle it
#else
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULI,d0		| denote as FP emulation trap
	jra	fault			| do it
#endif
#else
	jra	_illinst
#endif

_fpunsupp:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	_illinst		| no, treat as illinst
#ifdef FPSP
	.globl	fpsp_unsupp
	jmp	fpsp_unsupp		| yes, go handle it
#else
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULD,d0		| denote as FP emulation trap
	jra	fault			| do it
#endif
#else
	jra	_illinst
#endif

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
	.globl	_fpfault
_fpfault:
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-	| save user registers
	movl	usp,a0		| and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	movl	_curpcb,a0	| current pcb
	lea	a0@(PCB_FPCTX),a0 | address of FP savearea
	fsave	a0@		| save state
#if defined(M68040) || defined(M68060)
	/* always null state frame on 68040, 68060 */
	cmpl	#FPU_68040,_fputype
	jle	Lfptnull 
#endif
	tstb	a0@		| null state frame?
	jeq	Lfptnull	| yes, safe
	clrw	d0		| no, need to tweak BIU
	movb	a0@(1),d0	| get frame size
	bset	#3,a0@(0,d0:w)	| set exc_pend bit of BIU
Lfptnull:
	fmovem	fpsr,sp@-	| push fpsr as code argument
	frestore a0@		| restore state
	movl	#T_FPERR,sp@-	| push type arg
	jra	_ASM_LABEL(faultstkadj) | call trap and deal with stack cleanup

/*
 * Other exceptions only cause four and six word stack frame and require
 * no post-trap stack adjustment.
 */

	.globl	_straytrap
_badtrap:
	moveml	#0xC0C0,sp@-		| save scratch regs
	movw	sp@(22),sp@-		| push exception vector info
	clrw	sp@-
	movl	sp@(22),sp@-		| and PC
	jbsr	_straytrap		| report
	addql	#8,sp			| pop args
	moveml	sp@+,#0x0303		| restore regs
	jra	rei			| all done

	.globl	_syscall
_trap0:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	d0,sp@-			| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall arg
	tstl	_astpending
	jne	Lrei2
	tstb	_ssir
	jeq	Ltrap1
	movw	#SPL1,sr
	tstb	_ssir
	jne	Lsir1
Ltrap1:
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most registers
	addql	#8,sp			| pop SP and stack adjust
	rte

/*
 * Routines for traps 1 and 2.  The meaning of the two traps depends
 * on whether we are an HPUX compatible process or a native 4.3 process.
 * Our native 4.3 implementation uses trap 1 as sigreturn() and trap 2
 * as a breakpoint trap.  HPUX uses trap 1 for a breakpoint, so we have
 * to make adjustments so that trap 2 is used for sigreturn.
 */
_trap1:
#ifdef COMPAT_HPUX
	btst	#MDP_TRCB,mdpflag	| being traced by an HPUX process?
	jeq	sigreturn		| no, trap1 is sigreturn
	jra	_trace			| yes, trap1 is breakpoint
#else
	jra	sigreturn		| no, trap1 is sigreturn
#endif

_trap2:
#ifdef COMPAT_HPUX
	btst	#MDP_TRCB,mdpflag	| being traced by an HPUX process?
	jeq	_trace			| no, trap2 is breakpoint
	jra	sigreturn		| yes, trap2 is sigreturn
#else
	jra	_trace			| no, trap2 is breakpoint
#endif

/*
 * Trap 12 is the entry point for the cachectl "syscall" (both HPUX & BSD)
 *	cachectl(command, addr, length)
 * command in d0, addr in a1, length in d1
 */
	.globl	_cachectl
_trap12:
	movl	d1,sp@-			| push length
	movl	a1,sp@-			| push addr
	movl	d0,sp@-			| push command
	jbsr	_cachectl		| do it
	lea	sp@(12),sp		| pop args
	jra	rei			| all done

/*
 * Trap 15 is used for:
 *	- KGDB traps
 *	- trace traps for SUN binaries (not fully supported yet)
 * We just pass it on and let trap() sort it all out
 */
_trap15:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
#ifdef KGDB
	moveq	#T_TRAP15,d0
	movw	sp@(FR_HW),d1		| get PSW
	andw	#PSL_S,d1		| from user mode?
	jeq	fault			| yes, just a regular fault
	movl	d0,sp@-
	.globl	_kgdb_trap_glue
	jbsr	_kgdb_trap_glue		| returns if no debugger
	addl	#4,sp
#endif
	moveq	#T_TRAP15,d0
	jra	fault

/*
 * Hit a breakpoint (trap 1 or 2) instruction.
 * Push the code and treat as a normal fault.
 */
_trace:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
#ifdef KGDB
	moveq	#T_TRACE,d0
	movw	sp@(FR_HW),d1		| get SSW
	andw	#PSL_S,d1		| from user mode?
	jeq	fault			| no, regular fault
	movl	d0,sp@-
	jbsr	_kgdb_trap_glue		| returns if no debugger
	addl	#4,sp
#endif
	moveq	#T_TRACE,d0
	jra	fault

/*
 * The sigreturn() syscall comes here.  It requires special handling
 * because we must open a hole in the stack to fill in the (possibly much
 * larger) original stack frame.
 */
sigreturn:
	lea	sp@(-84),sp		| leave enough space for largest frame
	movl	sp@(84),sp@		| move up current 8 byte frame
	movl	sp@(88),sp@(4)
	movl	#84,sp@-		| default: adjust by 84 bytes
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	#SYS_sigreturn,sp@-	| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall#
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	lea	sp@(FR_HW),a1		| pointer to HW frame
	movw	sp@(FR_ADJ),d0		| do we need to adjust the stack?
	jeq	Lsigr1			| no, just continue
	moveq	#92,d1			| total size
	subw	d0,d1			|  - hole size = frame size
	lea	a1@(92),a0		| destination
	addw	d1,a1			| source
	lsrw	#1,d1			| convert to word count
	subqw	#1,d1			| minus 1 for dbf
Lsigrlp:
	movw	a1@-,a0@-		| copy a word
	dbf	d1,Lsigrlp		| continue
	movl	a0,a1			| new HW frame base
Lsigr1:
	movl	a1,sp@(FR_SP)		| new SP value
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	jra	rei			| all done

/*
 * Interrupt handlers.
 *
 * For auto-vectored interrupts, the CPU provides the
 * vector 0x18+level.  Note we count spurious interrupts,
 * but don't do anything else with them.
 *
 * _intrhand_autovec is the entry point for auto-vectored
 * interrupts.
 *
 * For vectored interrupts, we pull the pc, evec, and exception frame
 * and pass them to the vectored interrupt dispatcher.  The vectored
 * interrupt dispatcher will deal with strays.
 *
 * _intrhand_vectored is the entry point for vectored interrupts.
 */

#define INTERRUPT_SAVEREG	moveml  #0xC0C0,sp@-
#define INTERRUPT_RESTOREREG	moveml  sp@+,#0x0303

	.globl	_isrdispatch_autovec,_nmintr
	.globl	_isrdispatch_vectored

_spurintr:	/* Level 0 */
	addql	#1,_intrcnt+0
	addql	#1,_cnt+V_INTR
	jra	rei

_intrhand_autovec:	/* Levels 1 through 6 */
	INTERRUPT_SAVEREG
	movw	sp@(22),sp@-		| push exception vector
	clrw	sp@-
	jbsr	_isrdispatch_autovec	| call dispatcher
	addql	#4,sp
	INTERRUPT_RESTOREREG
	jra	rei			| all done

_lev7intr:	/* Level 7: NMI */
	addql	#1,_intrcnt+32
	clrl	sp@-
	moveml	#0xFFFF,sp@-		| save registers
	movl	usp,a0			| and save
	movl	a0,sp@(FR_SP)		|   the user stack pointer
	jbsr	_nmintr			| call handler: XXX wrapper
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and remaining registers
	addql	#8,sp			| pop SP and stack adjust
	jra	rei			| all done

	.globl	_intrhand_vectored
_intrhand_vectored:
	INTERRUPT_SAVEREG
	lea	sp@(16),a1		| get pointer to frame
	movl	a1,sp@-
	movw	sp@(26),d0
	movl	d0,sp@-			| push exception vector info
	movl	sp@(26),sp@-		| and PC
	jbsr	_isrdispatch_vectored	| call dispatcher
	lea	sp@(12),sp		| pop value args
	INTERRUPT_RESTOREREG
	jra	rei			| all done

#undef INTERRUPT_SAVEREG
#undef INTERRUPT_RESTOREREG

/*
 * Emulation of VAX REI instruction.
 *
 * This code deals with checking for and servicing ASTs
 * (profiling, scheduling) and software interrupts (network, softclock).
 * We check for ASTs first, just like the VAX.  To avoid excess overhead
 * the T_ASTFLT handling code will also check for software interrupts so we
 * do not have to do it here.  After identifing that we need an AST we
 * drop the IPL to allow device interrupts.
 *
 * This code is complicated by the fact that sendsig may have been called
 * necessitating a stack cleanup.
 */
	.comm	_ssir,1
	.globl	_astpending
	.globl	rei
rei:
	tstl	_astpending		| AST pending?
	jeq	Lchksir			| no, go check for SIR
Lrei1:
	btst	#5,sp@			| yes, are we returning to user mode?
	jne	Lchksir			| no, go check for SIR
	movw	#PSL_LOWIPL,sr		| lower SPL
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lrei2:
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_ASTFLT,sp@-		| type == async system trap
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(FR_ADJ),d0		| need to adjust stack?
	jne	Laststkadj		| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,sp			| toss SP and stack adjust
	rte				| and do real RTE
Laststkadj:
	lea	sp@(FR_HW),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(FR_SP)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	rte				| and do real RTE
Lchksir:
	tstb	_ssir			| SIR pending?
	jeq	Ldorte			| no, all done
	movl	d0,sp@-			| need a scratch register
	movw	sp@(4),d0		| get SR
	andw	#PSL_IPL7,d0		| mask all but IPL
	jne	Lnosir			| came from interrupt, no can do
	movl	sp@+,d0			| restore scratch register
Lgotsir:
	movw	#SPL1,sr		| prevent others from servicing int
	tstb	_ssir			| too late?
	jeq	Ldorte			| yes, oh well...
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lsir1:
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_SSIR,sp@-		| type == software interrupt
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and all remaining registers
	addql	#8,sp			| pop SP and stack adjust
	rte
Lnosir:
	movl	sp@+,d0			| restore scratch register
Ldorte:
	rte				| real return

/*
 * Primitives
 */ 

/*
 * Use common m68k support routines.
 */
#include <m68k/m68k/support.s>

	.globl	_whichqs,_qs,_cnt,_panic
	.globl	_curproc,_want_resched

/*
 * Use common m68k process manipulation routines.
 */
#include <m68k/m68k/proc_subr.s>

Lsw0:
	.asciz	"switch"
	.even

	.globl	_curpcb
	.globl	_masterpaddr	| XXX compatibility (debuggers)
	.data
_masterpaddr:			| XXX compatibility (debuggers)
_curpcb:
	.long	0
mdpflag:
	.byte	0		| copy of proc md_flags low byte
	.align	2
	.comm	nullpcb,SIZEOF_PCB
	.text

/*
 * At exit of a process, do a switch for the last time.
 * Switch to a safe stack and PCB, and deallocate the process's resources.
 */
ENTRY(switch_exit)
	movl    sp@(4),a0
	movl    #nullpcb,_curpcb        | save state into garbage pcb
	lea     tmpstk,sp               | goto a tmp stack

	/* Free old process's resources. */
	movl    #USPACE,sp@-            | size of u-area
	movl    a0@(P_ADDR),sp@-        | address of process's u-area
	movl    _kernel_map,sp@-        | map it was allocated in
	jbsr    _kmem_free              | deallocate it
	lea     sp@(12),sp              | pop args

	jra	_cpu_switch

/*
 * When no processes are on the runq, Swtch branches to Idle
 * to wait for something to come ready.
 */
	.globl	Idle
Idle:
	stop	#PSL_LOWIPL
	movw	#PSL_HIGHIPL,sr
	movl    _whichqs,d0
	jeq     Idle
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,sp@-
	jbsr	_panic
	/*NOTREACHED*/

/*
 * cpu_switch()
 *
 * NOTE: On the mc68851 (318/319/330) we attempt to avoid flushing the
 * entire ATC.  The effort involved in selective flushing may not be
 * worth it, maybe we should just flush the whole thing?
 *
 * NOTE 2: With the new VM layout we now no longer know if an inactive
 * user's PTEs have been changed (formerly denoted by the SPTECHG p_flag
 * bit).  For now, we just always flush the full ATC.
 */
ENTRY(cpu_switch)
	movl	_curpcb,a0		| current pcb
	movw	sr,a0@(PCB_PS)		| save sr before changing ipl
#ifdef notyet
	movl	_curproc,sp@-		| remember last proc running
#endif
	clrl	_curproc

	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	movw    #PSL_HIGHIPL,sr         | lock out interrupts
	movl    _whichqs,d0
	jeq     Idle
Lsw1:
	movl    d0,d1
	negl    d0
	andl    d1,d0
	bfffo   d0{#0:#32},d1
	eorib   #31,d1

	movl    d1,d0
	lslb    #3,d1                   | convert queue number to index
	addl    #_qs,d1                 | locate queue (q)
	movl    d1,a1
	movl    a1@(P_FORW),a0          | p = q->p_forw
	cmpal   d1,a0                   | anyone on queue?
	jeq     Lbadsw                  | no, panic
	movl    a0@(P_FORW),a1@(P_FORW) | q->p_forw = p->p_forw
	movl    a0@(P_FORW),a1          | n = p->p_forw
	movl    d1,a1@(P_BACK)          | n->p_back = q
	cmpal   d1,a1                   | anyone left on queue?
	jne     Lsw2                    | yes, skip
	movl    _whichqs,d1
	bclr    d0,d1                   | no, clear bit
	movl    d1,_whichqs
Lsw2:
	movl	a0,_curproc
	clrl	_want_resched
#ifdef notyet
	movl	sp@+,a1
	cmpl	a0,a1			| switching to same proc?
	jeq	Lswdone			| yes, skip save and restore
#endif
	/*
	 * Save state of previous process in its pcb.
	 */
	movl	_curpcb,a1
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
	movl	usp,a2			| grab USP (a2 has been saved)
	movl	a2,a1@(PCB_USP)		| and save it

	tstl	_fputype		| Do we have an FPU?
	jeq	Lswnofpsave		| No  Then don't attempt save.
	lea	a1@(PCB_FPCTX),a2	| pointer to FP save area
	fsave	a2@			| save FP state
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lsavfp60                
#endif  
	tstb	a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a2@(312)	| save FP control registers
#if defined(M68060)
	jra	Lswnofpsave 
Lsavfp60:
#endif  
#endif  
#if defined(M68060)
	tstb	a2@(2)			| null state frame?
	jeq	Lswnofpsave		| yes, all done 
	fmovem	fp0-fp7,a2@(216)	| save FP general registers 
	fmovem	fpcr,a2@(312)		| save FP control registers
	fmovem	fpsr,a2@(316)
	fmovem	fpi,a2@(320)
#endif
Lswnofpsave:

#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif
	clrl	a0@(P_BACK)		| clear back link
	movb	a0@(P_MD_FLAGS+3),mdpflag | low byte of p_md.md_flags
	movl	a0@(P_ADDR),a1		| get p_addr
	movl	a1,_curpcb

	/* see if pmap_activate needs to be called; should remove this */
	movl	a0@(P_VMSPACE),a0	| vmspace = p->p_vmspace
#ifdef DIAGNOSTIC
	tstl	a0			| map == VM_MAP_NULL?
	jeq	Lbadsw			| panic
#endif
	movl	a0@(VM_PMAP),a0		| pmap = vmspace->vm_map.pmap
	tstl	a0@(PM_STCHG)		| pmap->st_changed?
	jeq	Lswnochg		| no, skip
	pea	a1@			| push pcb (at p_addr)
	pea	a0@			| push pmap
	jbsr	_pmap_activate		| pmap_activate(pmap, pcb)
	addql	#8,sp
	movl	_curpcb,a1		| restore p_addr
Lswnochg:

	lea     tmpstk,sp               | now goto a tmp stack for NMI
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lres1a			| no, skip
	.word	0xf518			| yes, pflusha
	movl	a1@(PCB_USTP),d0	| get USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
	.long	0x4e7b0806		| movc d0,urp
	jra	Lcxswdone
Lres1a:
#endif
	movl	#CACHE_CLR,d0
	movc	d0,cacr			| invalidate cache(s)
	pflusha				| flush entire TLB
	movl	a1@(PCB_USTP),d0	| get USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load new user root pointer
Lcxswdone:
	moveml	a1@(PCB_REGS),#0xFCFC	| and registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			| and USP

	tstl	_fputype		| If we don't have an FPU,
	jeq	Lnofprest		|  don't try to restore it.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lresfp60rest1
#endif  
	tstb	a0@			| null state frame?
	jeq	Lresfprest		| yes, easy
#if defined(M68040)
	cmpl	#FPU_68040,_fputype	| 68040?
	jne	Lresnot040		| no, skip
	clrl	sp@-			| yes...
	frestore sp@+			| ...magic!
Lresnot040:
#endif
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
#if defined(M68060)
	jra     Lresfprest
Lresfp60rest1:
#endif  
#endif  
#if defined(M68060)
	tstb	a0@(2)			| null state frame?
	jeq	Lresfprest		| yes, easy
	fmovem	a0@(312),fpcr		| restore FP control registers
	fmovem	a0@(316),fpsr
	fmovem	a0@(320),fpi
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
#endif
Lresfprest:
	frestore a0@			| restore state
Lnofprest:
	movw	a1@(PCB_PS),sr		| no, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	sp@(4),a1
	movw	sr,a1@(PCB_PS)
	movl	usp,a0			| grab USP
	movl	a0,a1@(PCB_USP)		| and save it
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers

	tstl	_fputype		| Do we have FPU?
	jeq	Lsvnofpsave		| No?  Then don't save state.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	fsave	a0@			| save FP state
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype     
	jeq	Lsvsavfp60
#endif  
	tstb	a0@			| null state frame?
	jeq	Lsvnofpsave		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(312)	| save FP control registers
#if defined(M68060)
	jra	Lsvnofpsave
Lsvsavfp60:
#endif
#endif  
#if defined(M68060)
	tstb	a0@(2)			| null state frame?
	jeq	Lsvnofpsave		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr,a0@(312)		| save FP control registers
	fmovem	fpsr,a0@(316)
	fmovem	fpi,a0@(320)
#endif  
Lsvnofpsave:
	moveq	#0,d0			| return 0
	rts

#if defined(M68040)
ENTRY(suline)
	movl	sp@(4),a0		| address to write
	movl	_curpcb,a1		| current pcb
	movl	#Lslerr,a1@(PCB_ONFAULT) | where to return to on a fault
	movl	sp@(8),a1		| address of line
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	moveq	#0,d0			| indicate no fault
	jra	Lsldone
Lslerr:
	moveq	#-1,d0
Lsldone:
	movl	_curpcb,a1		| current pcb
	clrl	a1@(PCB_ONFAULT)	| clear fault address
	rts
#endif

/*
 * Invalidate entire TLB.
 */
ENTRY(TBIA)
__TBIA:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu3		| no, skip
	.word	0xf518			| yes, pflusha
	rts
Lmotommu3:
#endif
	tstl	_mmutype		| what mmu?
	jpl	Lmc68851a		| 68851 implies no d-cache
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
Lmc68851a:
	rts

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
ENTRY(TBIS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush entire TLB
#endif
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu4		| no, skip
	movl	sp@(4),a0
	movc	dfc,d1
	moveq	#1,d0			| user space
	movc	d0,dfc
	.word	0xf508			| pflush a0@
	moveq	#5,d0			| super space
	movc	d0,dfc
	.word	0xf508			| pflush a0@
	movc	d1,dfc
	rts
Lmotommu4:
#endif
	tstl	_mmutype		| is 68851?
	jpl	Lmc68851b		| 
	movl	sp@(4),a0		| get addr to flush
	pflush	#0,#0,a0@		| flush address from both sides
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip data cache
	rts
Lmc68851b:
	pflushs	#0,#0,a0@		| flush address from both sides
	rts

/*
 * Invalidate supervisor side of TLB
 */
ENTRY(TBIAS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
#if defined(M68040)
	cmpl    #MMU_68040,_mmutype     | 68040?
	jne     Lmotommu5               | no, skip
	.word   0xf518                  | yes, pflusha (for now) XXX
	rts
Lmotommu5:
#endif
	pflush	#4,#4			| flush supervisor TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

/*
 * Invalidate user side of TLB
 */
ENTRY(TBIAU)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
#if defined(M68040)
	cmpl    #MMU_68040,_mmutype     | 68040?
	jne     Lmotommu6               | no, skip
	.word   0xf518                  | yes, pflusha (for now) XXX
	rts
Lmotommu6:
#endif
	pflush	#0,#4			| flush user TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
#if defined(M68040)
ENTRY(ICPA)
	cmpl    #MMU_68040,_mmutype     | 68040
	jne     Lmotommu7               | no, skip
	.word   0xf498                  | cinva ic
	rts
Lmotommu7:
#endif
	movl	#IC_CLEAR,d0
	movc	d0,cacr			| invalidate i-cache
	rts

/*
 * Invalidate data cache.
 * NOTE: we do not flush 68030 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
ENTRY(DCIA)
__DCIA:
#if defined(M68040)
	cmpl    #MMU_68040,_mmutype     | 68040
	jne     Lmotommu8               | no, skip
	/* XXX implement */
	rts
Lmotommu8:
#endif
	rts

ENTRY(DCIS)
__DCIS:
#if defined(M68040)
	cmpl    #MMU_68040,_mmutype     | 68040
	jne     Lmotommu9               | no, skip
	/* XXX implement */
	rts
Lmotommu9:
#endif
	rts

ENTRY(DCIU)
__DCIU:
#if defined(M68040)
	cmpl    #MMU_68040,_mmutype     | 68040
	jne     LmotommuA               | no, skip
	/* XXX implement */
	rts
LmotommuA:
#endif
	rts

#if defined(M68040)
ENTRY(ICPL)
	movl    sp@(4),a0               | address
	.word   0xf488                  | cinvl ic,a0@
	rts
ENTRY(ICPP)
	movl    sp@(4),a0               | address
	.word   0xf490                  | cinvp ic,a0@
	rts
ENTRY(DCPL)
	movl    sp@(4),a0               | address
	.word   0xf448                  | cinvl dc,a0@
	rts
ENTRY(DCPP)
	movl    sp@(4),a0               | address
	.word   0xf450                  | cinvp dc,a0@
	rts
ENTRY(DCPA)
	.word   0xf458                  | cinva dc
	rts
ENTRY(DCFL)
	movl    sp@(4),a0               | address
	.word   0xf468                  | cpushl dc,a0@
	rts
ENTRY(DCFP)
	movl    sp@(4),a0               | address
	.word   0xf470                  | cpushp dc,a0@
	rts
#endif

ENTRY(PCIA)
#if defined(M68040)
ENTRY(DCFA)
	cmpl    #MMU_68040,_mmutype     | 68040
	jne     LmotommuB               | no, skip
	.word   0xf478                  | cpusha dc
	rts
LmotommuB:
#endif
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

ENTRY(ecacheon)
	rts

ENTRY(ecacheoff)
	rts

/*
 * Get callers current SP value.
 * Note that simply taking the address of a local variable in a C function
 * doesn't work because callee saved registers may be outside the stack frame
 * defined by A6 (e.g. GCC generated code).
 */
	.globl	_getsp
_getsp:
	movl	sp,d0			| get current SP
	addql	#4,d0			| compensate for return address
	rts

	.globl	_getsfc, _getdfc
_getsfc:
	movc	sfc,d0
	rts
_getdfc:
	movc	dfc,d0
	rts

/*
 * Load a new user segment table pointer.
 */
ENTRY(loadustp)
	movl	sp@(4),d0		| new USTP
	moveq	#PGSHIFT, d1
	lsll	d1,d0			| convert to addr
#if defined(M68040)
	cmpl    #MMU_68040,_mmutype     | 68040?
	jne     LmotommuC               | no, skip
	.long   0x4e7b0806              | movc d0,urp
	rts
LmotommuC:
#endif
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load root pointer
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts				|   since pmove flushes TLB

ENTRY(ploadw)
	movl	sp@(4),a0		| address to load
	ploadw	#1,a0@			| pre-load translation
	rts

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, spl0 requires special handling
 * as we need to check for our emulated software interrupts.
 */

ENTRY(spl0)
	moveq	#0,d0
	movw	sr,d0			| get old SR for return
	movw	#PSL_LOWIPL,sr		| restore new SR
	tstb	_ssir			| software interrupt pending?
	jeq	Lspldone		| no, all done
	subql	#4,sp			| make room for RTE frame
	movl	sp@(4),sp@(2)		| position return address
	clrw	sp@(6)			| set frame type 0
	movw	#PSL_LOWIPL,sp@		| and new SR
	jra	Lgotsir			| go handle it
Lspldone:
	rts

ENTRY(getsr)
	moveq	#0,d0
	movw	sr,d0
	rts

/*
 * _delay(unsigned N)
 *
 * Delay for at least (N/256) microseconds.
 * This routine depends on the variable:  delay_divisor
 * which should be set based on the CPU clock rate.
 */
	.globl	__delay
__delay:
	| d0 = arg = (usecs << 8)
	movl	sp@(4),d0
	| d1 = delay_divisor
	movl	_delay_divisor,d1
L_delay:
	subl	d1,d0
	jgt	L_delay
	rts

/*
 * Save and restore 68881 state.
 */
ENTRY(m68881_save)
	movl	sp@(4),a0		| save area pointer
	fsave	a0@			| save state
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lm68060fpsave
#endif
Lm68881fpsave:  
	tstb	a0@			| null state frame?
	jeq	Lm68881sdone		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(312)	| save FP control registers
Lm68881sdone:
	rts
#endif
#if defined(M68060)
Lm68060fpsave:
	tstb	a0@(2)			| null state frame?
	jeq	Lm68060sdone		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr,a0@(312)		| save FP control registers
	fmovem	fpsr,a0@(316)           
	fmovem	fpi,a0@(320)
Lm68060sdone:   
        rts
#endif  

ENTRY(m68881_restore)
	movl	sp@(4),a0		| save area pointer
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lm68060fprestore
#endif
Lm68881fprestore:
	tstb	a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lm68881rdone:
	frestore a0@			| restore state
	rts
#endif
#if defined(M68060)
Lm68060fprestore:
	tstb	a0@(2)			| null state frame?
	jeq	Lm68060fprdone		| yes, easy
	fmovem	a0@(312),fpcr		| restore FP control registers
	fmovem	a0@(316),fpsr
	fmovem	a0@(320),fpi
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lm68060fprdone:
	frestore a0@			| restore state
	rts
#endif

/*
 * Handle the nitty-gritty of rebooting the machine.
 * Basically we just turn off the MMU and jump to the appropriate ROM routine.
 */
	.globl	_doboot
_doboot:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jeq	Lnocache5		| yes, skip
#endif
	movl	#CACHE_OFF,d0
	movc	d0,cacr			| disable on-chip cache(s)
Lnocache5:
	movl	_boothowto,d0		| load howto
					| (used to load bootdev in d1 here)
	movl	sp@(4),d2		| arg
	lea	tmpstk,sp		| physical SP in case of NMI
	movl	#0,a7@-			| value for pmove to TC (turn off MMU)
	pmove	a7@,tc			| disable MMU
	movl	#0, d3
	movc	d3,vbr			| ROM VBR
	andl	#RB_SBOOT, d0		| mask off
	tstl	d0			| 
	bne	Lsboot			| sboot?
	/* NOT sboot */
	cmpl	#0, d2			| autoboot?
	beq	1f			| yes!
	trap	#15			| return to bug
	.short	MVMEPROM_EXIT		| exit
1:	movl	#0xff800004,a0		| restart the BUG
	movl	a0@, a0			| get PC
	jmp	a0@			| go!

Lsboot: /* sboot */
	cmpl	#0, d2			| autoboot?
	beq	1f			| yes!
	jmp 	0x4000			| back to sboot
1:	jmp	0x400a			| tell sboot to reboot us

	.data
	.globl	_machineid,_mmutype,_cputype,_fputype,_ectype,_protorp,_prototc
_machineid:
	.long	MVME_147	| default to MVME_147
_mmutype:
	.long	MMU_68030	| default to MMU_68030
_cputype:
	.long	CPU_68030	| default to CPU_68030
_fputype:
	.long	FPU_68882	| default to FPU_68882
_ectype:
	.long	EC_NONE		| external cache type, default to none
_protorp:
	.long	0,0		| prototype root pointer
_prototc:
	.long	0		| prototype translation control
	.globl	_bootpart,_bootdevlun,_bootctrllun,_bootaddr,_boothowto
_bootpart:
	.long	0
_bootdevlun:
	.long	0
_bootctrllun:
	.long	0
_bootaddr:
	.long	0
_boothowto:
	.long	0
	.globl	_cold
_cold:
	.long	1		| cold start flag
	.globl	_want_resched
_want_resched:
	.long	0
	.globl	_intiobase, _intiolimit
	.globl	_proc0paddr
_proc0paddr:
	.long	0		| KVA of proc0 u-area
_intiobase:
	.long	0		| KVA of base of internal IO space
_intiolimit:
	.long	0		| KVA of end of internal IO space
#ifdef DEBUG
	.globl	fulltflush, fullcflush
fulltflush:
	.long	0
fullcflush:
	.long	0
#endif
/* interrupt counters */
	.globl	_intrcnt,_eintrcnt,_intrnames,_eintrnames
_intrnames:
	.asciz	"spur"
	.asciz	"lev1"
	.asciz	"lev2"
	.asciz	"lev3"
	.asciz	"lev4"
	.asciz	"clock"
	.asciz	"lev6"
	.asciz	"nmi"
	.asciz	"statclock"
_eintrnames:
	.even
_intrcnt:
	.long	0,0,0,0,0,0,0,0,0,0
_eintrcnt:

#include <mvme68k/mvme68k/vectors.s>

/*	$NetBSD: frameasm.h,v 1.7.24.1 2007/11/06 23:17:41 matt Exp $	*/

#ifndef _I386_FRAMEASM_H_
#define _I386_FRAMEASM_H_

#ifdef _KERNEL_OPT
#include "opt_multiprocessor.h"
#endif

#ifndef TRAPLOG
#define TLOG		/**/
#else
/*
 * Fill in trap record
 */
#define TLOG						\
9:							\
	movl	%fs:CPU_TLOG_OFFSET, %eax;		\
	movl	%fs:CPU_TLOG_BASE, %ebx;		\
	addl	$SIZEOF_TREC,%eax;			\
	andl	$SIZEOF_TLOG-1,%eax;			\
	addl	%eax,%ebx;				\
	movl	%eax,%fs:CPU_TLOG_OFFSET;		\
	movl	%esp,TREC_SP(%ebx);			\
	movl	$9b,TREC_HPC(%ebx);			\
	movl	TF_EIP(%esp),%eax;			\
	movl	%eax,TREC_IPC(%ebx);			\
	rdtsc			;			\
	movl	%eax,TREC_TSC(%ebx);			\
	movl	$MSR_LASTBRANCHFROMIP,%ecx;		\
	rdmsr			;			\
	movl	%eax,TREC_LBF(%ebx);			\
	incl	%ecx		;			\
	rdmsr			;			\
	movl	%eax,TREC_LBT(%ebx);			\
	incl	%ecx		;			\
	rdmsr			;			\
	movl	%eax,TREC_IBF(%ebx);			\
	incl	%ecx		;			\
	rdmsr			;			\
	movl	%eax,TREC_IBT(%ebx)
#endif
		
/*
 * These are used on interrupt or trap entry or exit.
 */
#define	INTRENTRY \
	subl	$TF_PUSHSIZE,%esp	; \
	movl	%gs,TF_GS(%esp)	; \
	movl	%fs,TF_FS(%esp) ; \
	movl	%eax,TF_EAX(%esp)	; \
	movl	%es,TF_ES(%esp) ; \
	movl	%ds,TF_DS(%esp) ; \
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax	; \
	movl	%edi,TF_EDI(%esp)	; \
	movl	%esi,TF_ESI(%esp)	; \
	movl	%eax,%ds	; \
	movl	%ebp,TF_EBP(%esp)	; \
	movl	%eax,%es	; \
	movl	%ebx,TF_EBX(%esp)	; \
	movl	%eax,%gs	; \
	movl	%edx,TF_EDX(%esp)	; \
	movl	$GSEL(GCPU_SEL, SEL_KPL),%eax	; \
	movl	%ecx,TF_ECX(%esp)	; \
	movl	%eax,%fs	; \
	TLOG

/*
 * INTRFASTEXIT should be in sync with trap(), resume_iret and friends.
 */
#define	INTRFASTEXIT \
	movl	TF_GS(%esp),%gs	; \
	movl	TF_FS(%esp),%fs	; \
	movl	TF_ES(%esp),%es	; \
	movl	TF_DS(%esp),%ds	; \
	movl	TF_EDI(%esp),%edi	; \
	movl	TF_ESI(%esp),%esi	; \
	movl	TF_EBP(%esp),%ebp	; \
	movl	TF_EBX(%esp),%ebx	; \
	movl	TF_EDX(%esp),%edx	; \
	movl	TF_ECX(%esp),%ecx	; \
	movl	TF_EAX(%esp),%eax	; \
	addl	$(TF_PUSHSIZE+8),%esp	; \
	iret

#define	DO_DEFERRED_SWITCH \
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)		; \
	jz	1f					; \
	call	_C_LABEL(pmap_load)			; \
	1:

#define	CHECK_DEFERRED_SWITCH \
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)

#define	CHECK_ASTPENDING(reg)	movl	CPUVAR(CURLWP),reg	; \
				cmpl	$0, reg			; \
				je	1f			; \
				cmpl	$0, L_MD_ASTPENDING(reg); \
				1:
#define	CLEAR_ASTPENDING(reg)	movl	$0, L_MD_ASTPENDING(reg)

/*
 * IDEPTH_INCR:
 * increase ci_idepth and switch to the interrupt stack if necessary.
 * note that the initial value of ci_idepth is -1.
 *
 * => should be called with interrupt disabled.
 * => save the old value of %esp in %eax.
 */

#define	IDEPTH_INCR \
	incl	CPUVAR(IDEPTH); \
	movl	%esp, %eax; \
	jne	999f; \
	movl	CPUVAR(INTRSTACK), %esp; \
999:	pushl	%eax; \

/*
 * IDEPTH_DECR:
 * decrement ci_idepth and switch back to
 * the original stack saved by IDEPTH_INCR.
 *
 * => should be called with interrupt disabled.
 */

#define	IDEPTH_DECR \
	popl	%esp; \
	decl	CPUVAR(IDEPTH)

#endif /* _I386_FRAMEASM_H_ */

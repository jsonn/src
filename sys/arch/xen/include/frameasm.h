/*	$NetBSD: frameasm.h,v 1.1.4.3 2004/09/18 14:42:53 skrll Exp $	*/
/*	NetBSD: frameasm.h,v 1.4 2004/02/20 17:35:01 yamt Exp 	*/

#ifndef _I386_FRAMEASM_H_
#define _I386_FRAMEASM_H_

#ifdef _KERNEL_OPT
#include "opt_multiprocessor.h"
#endif

/* XXX assym.h */
#define TRAP_INSTR	int $0x82
#define __HYPERVISOR_stack_switch          4
#define __HYPERVISOR_fpu_taskswitch	   7

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
	cld; \
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

#define	DO_DEFERRED_SWITCH(reg) \
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)		; \
	jz	1f					; \
	call	_C_LABEL(pmap_load)			; \
	1:

#define	CHECK_DEFERRED_SWITCH(reg) \
	cmpl	$0, CPUVAR(WANT_PMAPLOAD)

#define	CHECK_ASTPENDING(reg)	movl	CPUVAR(CURLWP),reg	; \
				cmpl	$0, reg			; \
				je	1f			; \
				movl	L_PROC(reg),reg		; \
				cmpl	$0, P_MD_ASTPENDING(reg); \
				1:
#define	CLEAR_ASTPENDING(reg)	movl	$0, P_MD_ASTPENDING(reg)

#if !defined(XEN)
#define	CLI(reg)	cli
#define	STI(reg)	sti
#else
/* XXX assym.h */
#define	EVENTS 0
#define	EVENTS_MASK 4
#define EVENTS_MASTER_ENABLE_BIT  31
#define CLI(reg)	movl	_C_LABEL(HYPERVISOR_shared_info),reg ;	\
    			btrl	$EVENTS_MASTER_ENABLE_BIT,EVENTS_MASK(reg)
#define STI(reg)	movl	_C_LABEL(HYPERVISOR_shared_info),reg ;	\
    			btsl	$EVENTS_MASTER_ENABLE_BIT,EVENTS_MASK(reg)
#define STIC(reg)	movl	_C_LABEL(HYPERVISOR_shared_info),reg ;	\
    			btsl	$EVENTS_MASTER_ENABLE_BIT,EVENTS_MASK(reg) ; \
			movl	EVENTS(reg),reg ; \
			testl	reg,reg
#endif

#endif /* _I386_FRAMEASM_H_ */

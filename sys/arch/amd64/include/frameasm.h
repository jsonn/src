/*	$NetBSD: frameasm.h,v 1.4.14.1 2007/10/17 21:38:17 bouyer Exp $	*/

#ifndef _AMD64_MACHINE_FRAMEASM_H
#define _AMD64_MACHINE_FRAMEASM_H
#include "opt_xen.h"

/*
 * Macros to define pushing/popping frames for interrupts, traps
 * and system calls. Currently all the same; will diverge later.
 */

#ifdef XEN
#define HYPERVISOR_iret hypercall_page + (__HYPERVISOR_iret * 32)
/* Xen do not need swapgs, done by hypervisor */
#define swapgs
#define iretq	subq $8,%rsp ; jmp HYPERVISOR_iret
#endif

/*
 * These are used on interrupt or trap entry or exit.
 */
#define INTR_SAVE_GPRS \
	subq	$120,%rsp	; \
	movq	%r15,TF_R15(%rsp)	; \
	movq	%r14,TF_R14(%rsp)	; \
	movq	%r13,TF_R13(%rsp)	; \
	movq	%r12,TF_R12(%rsp)	; \
	movq	%r11,TF_R11(%rsp)	; \
	movq	%r10,TF_R10(%rsp)	; \
	movq	%r9,TF_R9(%rsp)		; \
	movq	%r8,TF_R8(%rsp)		; \
	movq	%rdi,TF_RDI(%rsp)	; \
	movq	%rsi,TF_RSI(%rsp)	; \
	movq	%rbp,TF_RBP(%rsp)	; \
	movq	%rbx,TF_RBX(%rsp)	; \
	movq	%rdx,TF_RDX(%rsp)	; \
	movq	%rcx,TF_RCX(%rsp)	; \
	movq	%rax,TF_RAX(%rsp)

#define	INTR_RESTORE_GPRS \
	movq	TF_R15(%rsp),%r15	; \
	movq	TF_R14(%rsp),%r14	; \
	movq	TF_R13(%rsp),%r13	; \
	movq	TF_R12(%rsp),%r12	; \
	movq	TF_R11(%rsp),%r11	; \
	movq	TF_R10(%rsp),%r10	; \
	movq	TF_R9(%rsp),%r9		; \
	movq	TF_R8(%rsp),%r8		; \
	movq	TF_RDI(%rsp),%rdi	; \
	movq	TF_RSI(%rsp),%rsi	; \
	movq	TF_RBP(%rsp),%rbp	; \
	movq	TF_RBX(%rsp),%rbx	; \
	movq	TF_RDX(%rsp),%rdx	; \
	movq	TF_RCX(%rsp),%rcx	; \
	movq	TF_RAX(%rsp),%rax	; \
	addq	$120,%rsp

#define	INTRENTRY \
	subq	$32,%rsp		; \
	testq	$SEL_UPL,56(%rsp)	; \
	je	98f			; \
	swapgs				; \
	movw	%gs,0(%rsp)		; \
	movw	%fs,8(%rsp)		; \
	movw	%es,16(%rsp)		; \
	movw	%ds,24(%rsp)		; \
98: 	INTR_SAVE_GPRS

#ifndef XEN
#define INTRFASTEXIT \
	INTR_RESTORE_GPRS 		; \
	testq	$SEL_UPL,56(%rsp)	; \
	je	99f			; \
	cli				; \
	swapgs				; \
	movw	0(%rsp),%gs		; \
	movw	8(%rsp),%fs		; \
	movw	16(%rsp),%es		; \
	movw	24(%rsp),%ds		; \
99:	addq	$48,%rsp		; \
	iretq

#define INTR_RECURSE_HWFRAME \
	movq	%rsp,%r10		; \
	movl	%ss,%r11d		; \
	pushq	%r11			; \
	pushq	%r10			; \
	pushfq				; \
	movl	%cs,%r11d		; \
	pushq	%r11			; \
	pushq	%r13			;

#else	/* !XEN */
/*
 * Disabling events before going to user mode sounds like a BAD idea
 * do no restore gs either, HYPERVISOR_iret will do a swapgs
 */
#define INTRFASTEXIT \
 	INTR_RESTORE_GPRS 		; \
 	testq	$SEL_UPL,56(%rsp)	; \
 	je	99f			; \
 	movw	8(%rsp),%fs		; \
 	movw	16(%rsp),%es		; \
 	movw	24(%rsp),%ds		; \
99:	addq	$48,%rsp		; \
 	iretq
  
/* We must fixup CS, as even kernel mode runs at CPL 3 */
#define INTR_RECURSE_HWFRAME \
 	movq	%rsp,%r10		; \
 	movl	%ss,%r11d		; \
 	pushq	%r11			; \
 	pushq	%r10			; \
 	pushfq				; \
 	movl	%cs,%r11d		; \
 	pushq	%r11			; \
 	andb	$0xfc,(%rsp)		; \
 	pushq	%r13			;
 
#endif	/* !XEN */
 

#define CHECK_ASTPENDING(reg)	cmpq	$0, reg				; \
				je	99f				; \
				cmpl	$0, L_MD_ASTPENDING(reg)	; \
				99:

#define CLEAR_ASTPENDING(reg)	movl	$0, L_MD_ASTPENDING(reg)

#ifdef XEN
#define CLI(reg1,reg2) \
 	movq CPUVAR(CPUID),reg1 ;		\
 	shlq $6,reg1 ;					\
 	movq _C_LABEL(HYPERVISOR_shared_info),reg2 ;	\
 	addq reg1,reg2 ;				\
 	movb $1,EVTCHN_UPCALL_MASK(reg2)
#define STI(reg1,reg2) \
 	movq CPUVAR(CPUID),reg1 ;		\
 	shlq $6,reg1 ;					\
 	movq _C_LABEL(HYPERVISOR_shared_info),reg2 ;	\
 	addq reg1,reg2 ;				\
 	movb $0,EVTCHN_UPCALL_MASK(reg2)
#else /* XEN */
#define CLI(reg1,reg2) cli
#define STI(reg1,reg2) sti
#endif	/* XEN */
#endif /* _AMD64_MACHINE_FRAMEASM_H */

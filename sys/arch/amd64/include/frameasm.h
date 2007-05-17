/*	$NetBSD: frameasm.h,v 1.2.2.1 2007/05/17 13:40:51 yamt Exp $	*/

#ifndef _AMD64_MACHINE_FRAMEASM_H
#define _AMD64_MACHINE_FRAMEASM_H

/*
 * Macros to define pushing/popping frames for interrupts, traps
 * and system calls. Currently all the same; will diverge later.
 */

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
	movw	%es,16(%rsp)		; \
	movw	%ds,24(%rsp)		; \
98: 	INTR_SAVE_GPRS

#define INTRFASTEXIT \
	INTR_RESTORE_GPRS 		; \
	testq	$SEL_UPL,56(%rsp)	; \
	je	99f			; \
	cli				; \
	swapgs				; \
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


#define CHECK_ASTPENDING(reg)	cmpq	$0, reg				; \
				je	99f				; \
				cmpl	$0, L_MD_ASTPENDING(reg)	; \
				99:

#define CLEAR_ASTPENDING(reg)	movl	$0, L_MD_ASTPENDING(reg)

#endif /* _AMD64_MACHINE_FRAMEASM_H */

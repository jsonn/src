/*	$NetBSD: linux_emuldata.h,v 1.1.10.2 2001/03/27 15:31:46 bouyer Exp $	*/

#ifndef _COMMON_LINUX_EMULDATA_H
#define _COMMON_LINUX_EMULDATA_H

/*
 * This is auxillary data the linux compat code
 * needs to do its work.  A pointer to it is
 * stored in the emuldata field of the proc
 * structure.
 */
struct linux_emuldata {
#if notyet
	sigset_t	ps_siginfo;		/* Which signals have a RT handler */
#endif
	int		debugreg[8];	/* GDB information for ptrace - for use, */
	                    		/* see ../arch/i386/linux_ptrace.c */
	caddr_t	p_break;			/* Processes' idea of break */	
};
#endif /* !_COMMON_LINUX_EMULDATA_H */

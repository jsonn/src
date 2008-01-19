/*	$NetBSD: pcb.h,v 1.4.26.2 2008/01/19 12:14:14 bouyer Exp $	*/

/*	$OpenBSD: pcb.h,v 1.6 2000/01/12 07:24:35 mickey Exp $	*/

/*
 * Copyright (c) 1999-2000 Michael Shalayeff
 * All rights reserved.
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _HPPA_PCB_H_
#define _HPPA_PCB_H_

#include <machine/reg.h>

struct pcb {
	u_int64_t	pcb_fpregs[HPPA_NFPREGS+1];
					/* not in the trapframe */
	u_int		pcb_onfault;	/* SW copy fault handler */
	pa_space_t	pcb_space;	/* copy pmap_space, for asm's sake */
	vaddr_t		pcb_uva;	/* KVA for U-area */
	u_int		pcb_ksp;	/* kernel sp for ctxsw */
};

struct md_coredump {
	struct reg md_reg;
	struct fpreg md_fpreg;
}; 


#endif /* _HPPA_PCB_H_ */

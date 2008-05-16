/*	$NetBSD: hd6446xintcvar.h,v 1.2.78.1 2008/05/16 02:22:32 yamt Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HPCSH_DEV_HD6446XINTCVAR_H_
#define _HPCSH_DEV_HD6446XINTCVAR_H_

#define _HD6446X_INTR_N		16

struct hd6446x_intrhand {
	int (*hh_func)(void *);
	void *hh_arg;
	int hh_ipl;
	uint16_t hh_imask;
};

extern struct hd6446x_intrhand hd6446x_intrhand[];
extern uint16_t hd6446x_ienable;

extern void hd6446x_intr_init(void);
extern void *hd6446x_intr_establish(int, int, int, int (*)(void *), void *);
extern void hd6446x_intr_disestablish(void *);
extern void hd6446x_intr_priority(int, int);

extern int hd6446x_intr_raise(int);
extern void hd6446x_intr_resume(int);

#endif /* !_HPCSH_DEV_HD6446XINTCVAR_H_ */

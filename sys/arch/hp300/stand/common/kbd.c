/*	$NetBSD: kbd.c,v 1.4.8.1 2005/04/29 11:28:10 kent Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*
 * Indirect keyboard driver for standalone ITE.
 */

#ifdef ITECONSOLE

#include <sys/param.h>

#include <lib/libsa/stand.h>

#include <hp300/stand/common/samachdep.h>
#include <hp300/stand/common/kbdvar.h>

#ifndef SMALL

/*
 * Function switch initialized by keyboard drivers.
 */
struct kbdsw *selected_kbd;

int
kbdgetc(void)
{

	return (selected_kbd != NULL) ? (*selected_kbd->k_getc)() : 0;
}

void
kbdnmi(void)
{

	if (selected_kbd != NULL)
		(*selected_kbd->k_nmi)();

	/*
	 * This is the only reasonable thing to do, unfortunately.
	 * Simply restarting the boot block by frobbing the stack and
	 * jumping to begin: doesn't properly reset variables that
	 * are in the data segment.
	 */
	printf("\nboot interrupted, resetting...\n");
	DELAY(1000000);
	call_req_reboot();
}

void
kbdinit(void)
{
	int i;

	selected_kbd = NULL;

	for (i = 0; kbdsw[i].k_init != NULL; i++) {
		if ((*kbdsw[i].k_init)()) {
			selected_kbd = &kbdsw[i];
			return;
		}
	}
}

#endif /* SMALL */

#endif /* ITECONSOLE */

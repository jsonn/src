/*	$NetBSD: sigset.c,v 1.1.2.2 2003/01/17 05:13:01 thorpej Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sigset.c,v 1.1.2.2 2003/01/17 05:13:01 thorpej Exp $");
#endif

#include "namespace.h"
#include <errno.h>
#include <signal.h>
#include <stddef.h>

sig_t
sigset(int sig, void (*disp)(int))
{
	sigset_t set, oset;
	struct sigaction sa, osa;

	osa.sa_handler = SIG_ERR;

	if (disp == SIG_HOLD) {
		/* Add sig to current signal mask. */
		if (sigemptyset(&set) != 0)
			goto out;
		if (sigaddset(&set, sig) != 0)
			goto out;
		if (sigprocmask(SIG_BLOCK, &set, &oset) != 0)
			goto out;

		if (sigismember(&oset, sig)) {
			/* Had been masked before, return SIG_HOLD. */
			osa.sa_handler = SIG_HOLD;
		} else {
			/* Return previous disposition. */
		 	(void)sigaction(sig, NULL, &osa);
		}
	} else if (disp == SIG_ERR) {
		errno = EINVAL;
	} else {
		/* Set up and install new disposition. */
		sa.sa_handler = disp;
		if (sigemptyset(&sa.sa_mask) != 0)
			goto out;
		sa.sa_flags = 0;

		if (sigaction(sig, &sa, &osa) != 0) {
			osa.sa_handler = SIG_ERR;
			goto out;
		}
			
		/* Delete sig from current signal mask. */
		if (sigemptyset(&set) != 0)
			return (SIG_ERR);
		if (sigaddset(&set, sig) != 0)
			return (SIG_ERR);
		if (sigprocmask(SIG_UNBLOCK, &set, &oset) != 0)
			return (SIG_ERR);

		/* If had been masked before, return SIG_HOLD. */
		if (sigismember(&oset, sig))
			osa.sa_handler = SIG_HOLD;
	}

 out:
	return (osa.sa_handler);
}

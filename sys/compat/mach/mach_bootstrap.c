/*	$NetBSD: mach_bootstrap.c,v 1.2.2.2 2002/12/11 06:37:26 thorpej Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus
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
__KERNEL_RCSID(0, "$NetBSD: mach_bootstrap.c,v 1.2.2.2 2002/12/11 06:37:26 thorpej Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/proc.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>
#include <compat/mach/mach_bootstrap.h>
#include <compat/mach/mach_errno.h>

int 
mach_bootstrap_look_up(p, msgh, maxlen, dst)
	struct proc *p;
	mach_msg_header_t *msgh;
	size_t maxlen;
	mach_msg_header_t *dst;
{
	mach_bootstrap_look_up_request_t req;
	mach_bootstrap_look_up_reply_t rep;
	int error;
	int msglen;
	const char service_name[] = "lookup\21"; /* XXX Why */
	int service_name_len;

	if ((error = copyin(msgh, &req, sizeof(req))) != 0)
		return error;

	bzero(&rep, sizeof(rep));

	/* The trailer is word aligned  */
	service_name_len = (sizeof(service_name) + 1) & ~0x7UL; 
	msglen = sizeof(rep.rep_msgh) + sizeof(rep.rep_count) + 
	    sizeof(rep.rep_bootstrap_port) + service_name_len *
	    sizeof(rep.rep_trailer);

	rep.rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
	    MACH_MSGH_BITS_COMPLEX;
	rep.rep_msgh.msgh_size = msglen - sizeof(rep.rep_trailer);
	rep.rep_msgh.msgh_local_port = req.req_msgh.msgh_local_port;
	rep.rep_msgh.msgh_id = req.req_msgh.msgh_id + 100;
	rep.rep_count = 1; /* XXX Why? */
	rep.rep_bootstrap_port = 0x21b; /* XXX Why? */
	strcpy((char *)&rep.rep_service_name, service_name); 
	/* XXX This is the trailer. We should find something better */
	rep.rep_service_name[service_name_len + 7] = 8;

	return MACH_MSG_RETURN(p, &rep, msgh, msglen, maxlen, dst);
}


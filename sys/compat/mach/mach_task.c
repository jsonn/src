/*	$NetBSD: mach_task.c,v 1.3.2.3 2002/12/11 06:37:32 thorpej Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: mach_task.c,v 1.3.2.3 2002/12/11 06:37:32 thorpej Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/exec.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_param.h>

#include <compat/mach/mach_types.h>
#include <compat/mach/mach_message.h>
#include <compat/mach/mach_clock.h>
#include <compat/mach/mach_errno.h>
#include <compat/mach/mach_task.h>
#include <compat/mach/mach_syscallargs.h>


int 
mach_task_get_special_port(p, msgh, maxlen, dst)
	struct proc *p;
	mach_msg_header_t *msgh;
	size_t maxlen;
	mach_msg_header_t *dst;
{
	mach_task_get_special_port_request_t req;
	mach_task_get_special_port_reply_t rep;
	int error;

	if ((error = copyin(msgh, &req, sizeof(req))) != 0)
		return error;

	bzero(&rep, sizeof(rep));

	rep.rep_msgh.msgh_bits = 
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
	    MACH_MSGH_BITS_COMPLEX;
	rep.rep_msgh.msgh_size = sizeof(rep) - sizeof(rep.rep_trailer);
	rep.rep_msgh.msgh_local_port = req.req_msgh.msgh_local_port;
	rep.rep_msgh.msgh_id = req.req_msgh.msgh_id + 100;
	rep.rep_msgh_body.msgh_descriptor_count = 1;	/* XXX why ? */
	rep.rep_special_port.name = 0x90f; /* XXX why? */
	rep.rep_special_port.disposition = 0x11; /* XXX why? */
	rep.rep_trailer.msgh_trailer_size = 8;

	return MACH_MSG_RETURN(p, &rep, msgh, sizeof(rep), maxlen, dst);
}

int 
mach_ports_lookup(p, msgh, maxlen, dst)
	struct proc *p;
	mach_msg_header_t *msgh;
	size_t maxlen;
	mach_msg_header_t *dst;
{
	mach_ports_lookup_request_t req;
	mach_ports_lookup_reply_t rep;
	struct exec_vmcmd evc;
	int error;

	if ((error = copyin(msgh, &req, sizeof(req))) != 0)
		return error;
	
	bzero(&evc, sizeof(evc));
	evc.ev_addr = 0x00008000;
	evc.ev_len = PAGE_SIZE;
	evc.ev_prot = UVM_PROT_RW;
	evc.ev_proc = *vmcmd_map_zero;
 
	if ((error = (*evc.ev_proc)(p, &evc)) != 0)
		return MACH_MSG_ERROR(p, msgh, &req, &rep, error, maxlen, dst);

	bzero(&rep, sizeof(rep));

	rep.rep_msgh.msgh_bits =
	    MACH_MSGH_REPLY_LOCAL_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
	    MACH_MSGH_BITS_COMPLEX;
	rep.rep_msgh.msgh_size = sizeof(rep) - sizeof(rep.rep_trailer);
	rep.rep_msgh.msgh_local_port = req.req_msgh.msgh_local_port;
	rep.rep_msgh.msgh_id = req.req_msgh.msgh_id + 100;
	rep.rep_msgh_body.msgh_descriptor_count = 1;	/* XXX why ? */
	rep.rep_init_port_set.address = (void *)evc.ev_addr;
	rep.rep_init_port_set.count = 3; /* XXX why ? */
	rep.rep_init_port_set.copy = 2; /* XXX why ? */
	rep.rep_init_port_set.disposition = 0x11; /* XXX why? */
	rep.rep_init_port_set.type = 2; /* XXX why? */
	rep.rep_init_port_set_count = 3; /* XXX why? */
	rep.rep_trailer.msgh_trailer_size = 8;

	return MACH_MSG_RETURN(p, &rep, msgh, sizeof(rep), maxlen, dst);
}



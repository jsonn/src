/*	$NetBSD: mach_port.h,v 1.1.2.3 2002/12/11 06:37:30 thorpej Exp $ */

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

#ifndef	_MACH_PORT_H_
#define	_MACH_PORT_H_

/* port_deallocate */

typedef struct {
	mach_msg_header_t req_msgh;
	mach_ndr_record_t req_ndr;
	mach_port_name_t req_name;
} mach_port_deallocate_request_t;

typedef struct {
	mach_msg_header_t rep_msgh;
	mach_ndr_record_t rep_ndr;
	mach_kern_return_t rep_retval;
	mach_msg_trailer_t rep_trailer;
} mach_port_deallocate_reply_t;

/* port_allocate */

typedef struct {
	mach_msg_header_t req_msgh;
	mach_ndr_record_t req_ndr;
	mach_port_right_t req_right;
} mach_port_allocate_request_t;  

typedef struct {
	mach_msg_header_t rep_msgh;
	mach_ndr_record_t rep_ndr;
	mach_kern_return_t rep_retval;
	mach_port_name_t rep_name;
	mach_msg_trailer_t rep_trailer;
} mach_port_allocate_reply_t;  

/* port_insert_right */

typedef struct {
	mach_msg_header_t req_msgh;
	mach_msg_body_t req_body;
	mach_msg_port_descriptor_t req_poly;
	mach_ndr_record_t req_ndr;
	mach_port_name_t req_name;
} mach_port_insert_right_request_t;  

typedef struct {
	mach_msg_header_t rep_msgh;
	mach_ndr_record_t rep_ndr;
	mach_kern_return_t rep_retval;
	mach_msg_trailer_t rep_trailer;
} mach_port_insert_right_reply_t;  

/* port_type */

typedef struct {
	mach_msg_header_t req_msgh;
	mach_ndr_record_t req_ndr;
	mach_port_name_t req_name;
} mach_port_type_request_t;  

typedef struct {
	mach_msg_header_t rep_msgh;
	mach_ndr_record_t rep_ndr;
	mach_kern_return_t rep_retval;
	mach_port_type_t rep_ptype;
	mach_msg_trailer_t rep_trailer;
} mach_port_type_reply_t;  

int mach_port_deallocate(struct proc *, mach_msg_header_t *, 
    size_t,  mach_msg_header_t *);
int mach_port_allocate(struct proc *, mach_msg_header_t *,
    size_t,  mach_msg_header_t *);
int mach_port_insert_right(struct proc *, mach_msg_header_t *,
    size_t,  mach_msg_header_t *);
int mach_port_type(struct proc *, mach_msg_header_t *,
    size_t,  mach_msg_header_t *);

#endif /* _MACH_PORT_H_ */

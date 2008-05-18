/*	$NetBSD: mach_types.h,v 1.23.72.1 2008/05/18 12:33:23 yamt Exp $	 */

/*-
 * Copyright (c) 2001-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and Emmanuel Dreyfus.
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

#ifndef	_MACH_TYPES_H_
#define	_MACH_TYPES_H_

typedef int mach_port_t;
typedef int mach_port_name_t;
typedef int mach_port_type_t;
typedef register_t mach_kern_return_t;
typedef int mach_clock_res_t;
typedef int mach_clock_id_t;
typedef int mach_boolean_t;
typedef int mach_sleep_type_t;
typedef int mach_absolute_time_t;
typedef int mach_integer_t;
typedef int mach_cpu_type_t;
typedef int mach_cpu_subtype_t;
typedef int mach_port_right_t;
typedef register_t mach_vm_address_t;
typedef int mach_vm_inherit_t;
typedef int mach_vm_prot_t;
typedef int mach_thread_state_flavor_t;
typedef unsigned int mach_natural_t;
typedef unsigned long mach_vm_size_t;
typedef uint64_t mach_memory_object_size_t;
typedef unsigned long mach_vm_offset_t;
typedef uint64_t mach_memory_object_offset_t;
typedef int mach_vm_region_flavor_t;
typedef int mach_vm_behavior_t;
typedef int mach_vm_sync_t;
typedef int mach_exception_type_t;
typedef int mach_exception_behavior_t;
typedef unsigned int mach_exception_mask_t;
typedef int mach_port_flavor_t;
typedef mach_natural_t mach_port_seqno_t;
typedef mach_natural_t mach_port_mscount_t;
typedef mach_natural_t mach_port_msgcount_t;
typedef mach_natural_t mach_port_rights_t;
typedef mach_natural_t mach_task_flavor_t;
typedef mach_natural_t mach_thread_flavor_t;
typedef int mach_policy_t;
typedef int mach_vm_machine_attribute_val_t;
typedef unsigned int mach_vm_machine_attribute_t;
typedef mach_natural_t mach_port_urefs_t;
typedef int mach_port_delta_t;


/*
 * This is called cproc_t in Mach (cthread_t in Darwin). It is a pointer to
 * a struct cproc (struct cthread in Darwin), which is stored in userland and
 * seems to be opaque to the kernel. The kernel just has to store and restore
 * it with cthread_self() (pthread_self() in Darwin) and _cthread_set_self()
 * (_pthread_set_self() in Darwin).
 */
typedef void *mach_cproc_t;

typedef struct mach_timebase_info {
	u_int32_t	numer;
	u_int32_t	denom;
} *mach_timebase_info_t;

typedef struct {
	u_int8_t       mig_vers;
	u_int8_t       if_vers;
	u_int8_t       reserved1;
	u_int8_t       mig_encoding;
	u_int8_t       int_rep;
	u_int8_t       char_rep;
	u_int8_t       float_rep;
	u_int8_t       reserved2;
} mach_ndr_record_t;

typedef struct {
	mach_integer_t seconds;
	mach_integer_t microseconds;
} mach_time_value_t;

#ifdef DEBUG_MACH
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif /* DEBUG_MACH */

#endif /* !_MACH_TYPES_H_ */

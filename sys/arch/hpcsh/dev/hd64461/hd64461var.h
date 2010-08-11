/*	$NetBSD: hd64461var.h,v 1.3.78.2 2010/08/11 22:52:08 yamt Exp $	*/

/*-
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
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

#ifndef _HPCSH_DEV_HD64461VAR_H_
#define _HPCSH_DEV_HD64461VAR_H_
/*
 * HD64461 register access macro.
 */
#define hd64461_reg_read_1(r)		(*((volatile uint8_t *)(r)))
#define hd64461_reg_write_1(r, v)	(*((volatile uint8_t *)(r)) = (v))
#define hd64461_reg_read_2(r)		(*((volatile uint16_t *)(r)))
#define hd64461_reg_write_2(r, v)	(*((volatile uint16_t *)(r)) = (v))

/*
 * HD64461 modules canonical ID.
 */
enum hd64461_module_id {
	HD64461_MODULE_INTERFACE,
	HD64461_MODULE_INTC,
	HD64461_MODULE_POWER,
	HD64461_MODULE_TIMER,
	HD64461_MODULE_VIDEO,
	HD64461_MODULE_PCMCIA,
	HD64461_MODULE_GPIO,
	HD64461_MODULE_AFE,
	HD64461_MODULE_UART,
	HD64461_MODULE_FIR
};

struct hd64461_attach_args {
	enum hd64461_module_id  ha_module_id;
};

extern int use_afeck;

/*
 * Interrupt staff.
 */
#include <hpcsh/dev/hd6446x/hd6446xintcvar.h>

#endif /* !_HPCSH_DEV_HD64461VAR_H_ */

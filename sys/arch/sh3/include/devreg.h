/*	$NetBSD: devreg.h,v 1.1.6.3 2002/06/23 17:40:38 jdolecek Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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

#ifndef _SH3_DEVREG_H_
#define	_SH3_DEVREG_H_
/*
 * SH embeded device register defines.
 */

/*
 * Access method
 */
#define	_reg_read_1(a)		(*(__volatile__ u_int8_t *)((vaddr_t)(a)))
#define	_reg_read_2(a)		(*(__volatile__ u_int16_t *)((vaddr_t)(a)))
#define	_reg_read_4(a)		(*(__volatile__ u_int32_t *)((vaddr_t)(a)))
#define	_reg_write_1(a, v)						\
	(*(__volatile__ u_int8_t *)(a) = (u_int8_t)(v))
#define	_reg_write_2(a, v)						\
	(*(__volatile__ u_int16_t *)(a) = (u_int16_t)(v))
#define	_reg_write_4(a, v)						\
	(*(__volatile__ u_int32_t *)(a) = (u_int32_t)(v))

/*
 * Register address.
 */
#if defined(SH3) && defined(SH4)
#define	SH_(x)		__sh_ ## x
#elif defined(SH3)
#define	SH_(x)		SH3_ ## x
#elif defined(SH4)
#define	SH_(x)		SH4_ ## x
#endif

#ifndef _LOCORE
/* Initialize register address for SH3 && SH4 kernel. */
void sh_devreg_init(void);
#endif
#endif /* !_SH3_DEVREG_H_ */

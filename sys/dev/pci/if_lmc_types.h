/*	$NetBSD: if_lmc_types.h,v 1.3.2.2 2002/06/23 17:47:40 jdolecek Exp $	*/

/*-
 * Copyright (c) 1997-1999 LAN Media Corporation (LMC)
 * All rights reserved.  www.lanmedia.com
 *
 * This code is written by Michael Graff <graff@vix.com> for LMC.
 * The code is derived from permitted modifications to software created
 * by Matt Thomas (matt@3am-software.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All marketing or advertising materials mentioning features or
 *    use of this software must display the following acknowledgement:
 *      This product includes software developed by LAN Media Corporation
 *      and its contributors.
 * 4. Neither the name of LAN Media Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAN MEDIA CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LMC_TYPES_H
#define LMC_TYPES_H

/*
 * NetBSD uses _KERNEL, FreeBSD uses KERNEL.
 */
#if defined(_KERNEL) && defined(__NetBSD__)
#define LMC_IS_KERNEL
#endif
#if defined(KERNEL) && defined(__FreeBSD__)
#define LMC_IS_KERNEL
#endif
#if defined(__KERNEL__) && defined(linux)
#define LMC_IS_KERNEL
#endif

#if defined(LMC_IS_KERNEL)

/*
 * LMC has weird endianness issues, so we can't use the tulip_desc_t.
 */
typedef struct {
    u_int32_t d_status;
    u_int32_t d_ctl;
    u_int32_t d_addr1;
    u_int32_t d_addr2;
} lmc_desc_t;

#define	LMC_CTL_FLGS(x)		(((x)>>22)&0x3ff)
#define	LMC_CTL_LEN2(x)		(((x)>>11)&0x7ff)
#define	LMC_CTL_LEN1(x)		((x)&0x7ff)
#define	LMC_CTL(f,l1,l2)	((((f)&0x3ff)<<22)|(((l2)&0x7ff)<<11)|((l1)&0x7ff))

#if defined(__NetBSD__)
typedef bus_addr_t lmc_csrptr_t;
#else
typedef volatile u_int32_t *lmc_csrptr_t;
#endif

#if defined(__NetBSD__)
#define	lmc_intrfunc_t	int
#endif /* __NetBSD__ */

#if defined(__FreeBSD__)
#define	lmc_intrfunc_t	void
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#define	lmc_intrfunc_t	int
#endif /* __bsdi__ */

typedef struct lmc___softc lmc_softc_t;
typedef struct lmc___media lmc_media_t;
typedef struct lmc_ringinfo lmc_ringinfo_t;

#endif /* LMC_IS_KERNEL */

typedef struct lmc___ctl lmc_ctl_t;

#endif /* LMC_TYPES_H */

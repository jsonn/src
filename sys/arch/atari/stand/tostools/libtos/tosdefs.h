/*	$NetBSD: tosdefs.h,v 1.3.122.1 2008/05/16 02:22:06 yamt Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Leo Weppelman.
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

#ifndef _LIBTOS_TOSDEFS_H
#define _LIBTOS_TOSDEFS_H

#include <sys/types.h>


#include "kparamb.h"

/*
 * TOS variables in low memory
 */
struct _gem_mupb;
struct _basepage;
struct _osheader {
	u_int16_t	 os_entry;
	u_int16_t	 os_version;
	void		 *reseth;
	struct _osheader *os_beg;
	void		 *os_end;
	u_int32_t	 os_rsv1;
	struct _gem_mupb *os_magic;
	u_int32_t	 os_date;
	u_int16_t	 os_conf;
	u_int16_t	 os_dosdate;
	char		 **p_root;
	u_int8_t	 **pkbshift;
	struct _basepage **p_run;
	char		 *p_rsv2;
};

typedef	struct _osheader	OSH;

#define	ADDR_P_COOKIE	((long**)0x5a0)	/* Pointer to cookie jar	*/
#define	ADDR_OSHEAD	((OSH**)0x4f2)	/* Pointer Os-header		*/
#define	ADDR_PHYSTOP	((long*)0x42e)	/* End of ST-ram		*/
#define	ADDR_RAMTOP	((long*)0x5a4)	/* End of TT-ram (unofficial)	*/
#define	ADDR_CHKRAMTOP	((long*)0x5a8)	/*  above is valid (unofficial)	*/

#define	RAMTOP_MAGIC	(0x1357bd13)	/* Magic no. for ADDR_CHKRAMTOP	*/

#define	TTRAM_BASE	(0x1000000)	/* Fastram always starts here	*/
#define	CTRAM_BASE	(0x4000000)	/*  ... except on CT2 boards:	*/
					/*         Logical : TTRAM_BASE */
					/*         Physical: CTRAM_BASE */

/*
 * Kernel parameter block
 */
typedef struct osdsc {
	const char *	ostype;
	const char *	osname;
	unsigned	rootfs;
	struct kparamb 	kp;
} osdsc_t;

#define	ksize		kp.ksize
#define	kstart		kp.kp
#define	kentry		kp.entry
#define	k_esym		kp.esym_loc
#define	stmem_size	kp.stmem_size
#define	ttmem_size	kp.ttmem_size
#define	ttmem_start	kp.ttmem_start
#define	cputype		kp.bootflags
#define	boothowto	kp.boothowto

#endif /* !_LIBTOS_TOSDEFS_H */

/*	$NetBSD: hpux_exec.h,v 1.9.2.2 2001/11/18 00:07:49 gmcgarry Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: hpux_exec.h 1.6 92/01/20$
 *
 *	@(#)hpux_exec.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _HPUX_EXEC_H_
#define _HPUX_EXEC_H_

/*
 * HPUX a.out header format
 */
struct hpux_exec {
	long	ha_magic;	/* magic number */
	short	ha_version;	/* version ID */
	short	ha_shlhw;	/* shared lib "highwater" mark */
	long	ha_misc;	/* misc. info */
	long	ha_text;	/* size of text segment */
	long	ha_data;	/* size of initialized data */
	long	ha_bss;		/* size of uninitialized data */
	long	ha_trsize;	/* size of text relocation */
	long	ha_drsize;	/* size of data relocation */
	long	ha_pascal;	/* pascal section size */
	long	ha_symbols;	/* symbol table size */
	long	ha_pad0;
	long	ha_entry;	/* entry point */
	long	ha_pad1;
	long	ha_supsyms;	/* supplementary symbol table */
	long	ha_drelocs;	/* non-PIC relocation info */
	long	ha_extensions;	/* file offset of special extensions */
};

#define	HPUX_EXEC_HDR_SIZE	(sizeof(struct hpux_exec))

#define	HPUX_MAGIC(ha)		((ha)->ha_magic & 0xffff)
#define	HPUX_SYSID(ha)		(((ha)->ha_magic >> 16) & 0xffff)

/*
 * Additional values for HPUX_MAGIC()
 */
#define	HPUX_MAGIC_RELOC	0x0106		/* relocatable object */
#define HPUX_MAGIC_DL		0x010d		/* dynamic load library */
#define	HPUX_MAGIC_SHL		0x010e		/* shared library */

#define HPUX_LDPGSZ		4096		/* align to this */
#define HPUX_LDPGSHIFT		12		/* log2(HPUX_LDPGSZ) */

#define	HPUX_SEGMENT_ROUND(x)						\
	(((x) + HPUX_LDPGSZ - 1) & ~(HPUX_LDPGSZ - 1))

#define	HPUX_TXTOFF(x, m)						\
	((((m) == ZMAGIC) ||						\
	  ((m) == HPUX_MAGIC_SHL) ||					\
	  ((m) == HPUX_MAGIC_DL)) ?					\
	  HPUX_LDPGSZ : HPUX_EXEC_HDR_SIZE)

#define	HPUX_DATAOFF(x, m)						\
	((((m) == ZMAGIC) ||						\
	  ((m) == HPUX_MAGIC_SHL) ||					\
	  ((m) == HPUX_MAGIC_DL)) ?					\
	  (HPUX_LDPGSZ + HPUX_SEGMENT_ROUND((x).ha_text)) :		\
	  (HPUX_EXEC_HDR_SIZE + (x).ha_text))

#define	HPUX_PASOFF(x, m)						\
	((((m) == ZMAGIC) ||						\
	  ((m) == HPUX_MAGIC_SHL) ||					\
	  ((m) == HPUX_MAGIC_DL)) ?					\
	  (HPUX_LDPGSZ + HPUX_SEGMENT_ROUND((x).ha_text) +		\
	    HPUX_SEGMENT_ROUND((x).ha_data)) :				\
	  (HPUX_EXEC_HDR_SIZE + (x).ha_text + (x).ha_data))

#define	HPUX_SYMOFF(x, m)	(HPUX_PASOFF((x), (m)) + (x).ha_pascal)
#define	HPUX_SUPSYMOFF(x, m)	(HPUX_SYMOFF((x), (m)) + (x).ha_symbols)
#define	HPUX_RTEXTOFF(x, m)	(HPUX_SUPSYMOFF((x), (m)) + (x).ha_supsyms)
#define	HPUX_RDATAOFF(x, m)	(HPUX_RTEXTOFF((x), (m)) + (x).ha_trsize)
#define	HPUX_EXTOFF(x, m)	((x).ha_extensions)

#define	HPUXM_VALID	0x00000001
#define HPUXM_STKWT	0x02000000
#define HPUXM_DATAWT	0x04000000

int	exec_hpux_makecmds __P((struct proc *, struct exec_package *));
void	hpux_setregs __P((struct lwp *, struct exec_package *, u_long));

extern const struct emul emul_hpux;

#endif /* _HPUX_EXEC_H_ */

/*	$NetBSD: acvar.h,v 1.4.34.1 2001/10/10 11:56:04 fvdl Exp $	*/

/*
 * Copyright (c) 1991 University of Utah.
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
 * from: Utah $Hdr: acvar.h 1.1 91/06/19$
 *
 *	@(#)acvar.h	8.1 (Berkeley) 6/10/93
 */

struct	ac_softc {
	struct	device sc_dev;
	int	sc_target;
	int	sc_lun;
	int	sc_flags;
	struct	buf *sc_bp;
	struct	scsi_fmt_cdb *sc_cmd;
	struct	acinfo sc_einfo;
	short	sc_picker;
	struct	scsiqueue sc_sq;
};

#define	ACF_ALIVE	0x01
#define ACF_OPEN	0x02
#define ACF_ACTIVE	0x04

#define ACCMD_INITES	0x07
#define	ACCMD_MODESENSE	0x1A
#define ACCMD_READES	0xB8
#define ACCMD_MOVEM	0xA5

struct	ac_restathdr {
	short	ac_felt;	/* first element reported */
	short	ac_nelt;	/* number of elements reported */
	long	ac_bcount;	/* length of report (really only 24 bits) */
};

struct	ac_restatphdr {
	char	ac_type;	/* type code */
	char	ac_res;
	short	ac_dlen;	/* element descriptor length */
	long	ac_bcount;	/* byte count (really only 24 bits) */
};

struct	ac_restatdb {
	short	ac_eaddr;	/* element address */
	u_int	ac_res1:2,
		ac_ie:1,	/* import enabled (IEE only) */
		ac_ee:1,	/* export enabled (IEE only) */
		ac_acc:1,	/* accessible from MTE */
		ac_exc:1,	/* element in abnormal state */
		ac_imp:1,	/* 1 == user inserted medium (IEE only) */
		ac_full:1;	/* element contains media */
};

#ifdef _KERNEL
struct vnode;
int	accommand __P((struct vnode *, int, char *, int));

void	acstart __P((void *));
void	acgo __P((void *));
void	acintr __P((void *, int));

int	acgeteinfo __P((struct vnode *));
void	acconvert __P((char *, char *, int));
#endif /* _KERNEL */

/*	$NetBSD: ufs_bswap.h,v 1.4.2.1 2000/11/20 18:11:54 bouyer Exp $	*/

/*
 * Copyright (c) 1998 Manuel Bouyer.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_ffs.h"
#endif

#include <machine/bswap.h>

/* Macros to access UFS flags */
#ifdef FFS_EI
#define	UFS_MPNEEDSWAP(mp)	(VFSTOUFS(mp)->um_flags & UFS_NEEDSWAP)
#define UFS_FSNEEDSWAP(fs)	((fs)->fs_flags & FS_SWAPPED)
#define	UFS_IPNEEDSWAP(ip)	UFS_MPNEEDSWAP(ITOV(ip)->v_mount)
#else
#define	UFS_MPNEEDSWAP(mp) (0)
#define UFS_FSNEEDSWAP(fs) (0)
#define	UFS_IPNEEDSWAP(ip) (0)
#endif

#if !defined(_KERNEL) || defined(FFS_EI)
/* inlines for access to swaped datas */
static __inline u_int16_t ufs_rw16 __P((u_int16_t, int));
static __inline u_int32_t ufs_rw32 __P((u_int32_t, int));
static __inline u_int64_t ufs_rw64 __P((u_int64_t, int));

static __inline u_int16_t
ufs_rw16(a, ns)
	u_int16_t a;
	int ns;
{
	return ((ns) ?  bswap16(a) : (a));
}
static __inline u_int32_t
ufs_rw32(a, ns)
	u_int32_t a;
	int ns;
{
	return ((ns) ?  bswap32(a) : (a));
}
static __inline u_int64_t
ufs_rw64(a, ns)
	u_int64_t a;
	int ns;
{
	return ((ns) ?  bswap64(a) : (a));
}
#else
#define ufs_rw16(a, ns) (a)
#define ufs_rw32(a, ns) (a)
#define ufs_rw64(a, ns) (a)
#endif

#define ufs_add16(a, b, ns) \
	(a) = ufs_rw16(ufs_rw16((a), (ns)) + (b), (ns))
#define ufs_add32(a, b, ns) \
	(a) = ufs_rw32(ufs_rw32((a), (ns)) + (b), (ns))
#define ufs_add64(a, b, ns) \
	(a) = ufs_rw64(ufs_rw64((a), (ns)) + (b), (ns))

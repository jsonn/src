/*	$NetBSD: ufs_wapbl.h,v 1.1.2.3 2008/07/03 16:40:26 simonb Exp $	*/

/*-
 * Copyright (c) 2003,2006,2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
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


#ifndef _UFS_UFS_UFS_WAPBL_H_
#define _UFS_UFS_UFS_WAPBL_H_

#if defined(_KERNEL_OPT)
#include "opt_wapbl.h"
#endif

#define	UFS_WAPBL_VERSION			1

#define	UFS_WAPBL_JOURNALLOC_END_PARTITION	1
#define	UFS_WAPBL_JOURNALLOC_IN_FILESYSTEM	2

#if defined(WAPBL)

#if defined(WAPBL_DEBUG)
#define WAPBL_DEBUG_INODES
#endif

int	wapbl_ufs_rename(void *v);

#ifdef WAPBL_DEBUG_INODES
void	ufs_wapbl_verify_inodes(struct mount *, const char *);
#endif

static __inline int
ufs_wapbl_begin2(struct mount *mp, struct vnode *vp1, struct vnode *vp2,
		 const char *file, int line)
{
	if (mp->mnt_wapbl) {
		int error;

		if (vp1)
			vref(vp1);
		if (vp2)
			vref(vp2);
		error = wapbl_begin(mp->mnt_wapbl, file, line);
		if (error)
			return error;
#ifdef WAPBL_DEBUG_INODES
		if (mp->mnt_wapbl->wl_lock.lk_exclusivecount == 1)
			ufs_wapbl_verify_inodes(mp, "wapbl_begin");
#endif
	}
	return 0;
}

static __inline void
ufs_wapbl_end2(struct mount *mp, struct vnode *vp1, struct vnode *vp2)
{
	if (mp->mnt_wapbl) {
#ifdef WAPBL_DEBUG_INODES
		if (mp->mnt_wapbl->wl_lock.lk_exclusivecount == 1)
			ufs_wapbl_verify_inodes(mp, "wapbl_end");
#endif
		wapbl_end(mp->mnt_wapbl);
		if (vp2)
			vrele(vp2);
		if (vp1)
			vrele(vp1);
	}
}

#define	UFS_WAPBL_BEGIN(mp)						\
	ufs_wapbl_begin2(mp, 0, 0, __FUNCTION__, __LINE__)
#define	UFS_WAPBL_BEGIN1(mp, v1)					\
	ufs_wapbl_begin2(mp, v1, 0, __FUNCTION__, __LINE__)
#define	UFS_WAPBL_END(mp)	ufs_wapbl_end2(mp, 0, 0)
#define	UFS_WAPBL_END1(mp, v1)	ufs_wapbl_end2(mp, v1, 0)

#define UFS_WAPBL_UPDATE(vp, access, modify, flags)			\
	if ((vp)->v_mount->mnt_wapbl) {					\
		UFS_UPDATE(vp, access, modify, flags);			\
	}

#ifdef UFS_WAPBL_DEBUG_JLOCK
#define	UFS_WAPBL_JLOCK_ASSERT(mp)					\
	if (mp->mnt_wapbl) wapbl_jlock_assert(mp->mnt_wapbl)
#define	UFS_WAPBL_JUNLOCK_ASSERT(mp)					\
	if (mp->mnt_wapbl) wapbl_junlock_assert(mp->mnt_wapbl)
#else
#define	UFS_WAPBL_JLOCK_ASSERT(mp)
#define	UFS_WAPBL_JUNLOCK_ASSERT(mp)
#endif

#define UFS_WAPBL_REGISTER_INODE(mp, ino, mode)				\
	if (mp->mnt_wapbl) wapbl_register_inode(mp->mnt_wapbl, ino, mode)
#define UFS_WAPBL_UNREGISTER_INODE(mp, ino, mode)			\
	if (mp->mnt_wapbl) wapbl_unregister_inode(mp->mnt_wapbl, ino, mode)

#define UFS_WAPBL_REGISTER_DEALLOCATION(mp, blk, len)			\
	if (mp->mnt_wapbl) wapbl_register_deallocation(mp->mnt_wapbl, blk, len)

#else /* ! WAPBL */
#define UFS_WAPBL_BEGIN(mp) 0
#define UFS_WAPBL_BEGIN1(mp, v1) 0
#define UFS_WAPBL_END(mp)	do { } while (0)
#define UFS_WAPBL_END1(mp, v1)
#define UFS_WAPBL_UPDATE(vp, access, modify, flags)	do { } while (0)
#define UFS_WAPBL_JLOCK_ASSERT(mp)
#define UFS_WAPBL_JUNLOCK_ASSERT(mp)
#define UFS_WAPBL_REGISTER_INODE(mp, ino, mode)
#define UFS_WAPBL_UNREGISTER_INODE(mp, ino, mode)
#define UFS_WAPBL_REGISTER_DEALLOCATION(mp, blk, len)
#endif

#endif /* !_UFS_UFS_UFS_WAPBL_H_ */

/* 	$NetBSD: devfs.h,v 1.1.14.2 2008/03/15 13:32:50 mjf Exp $ */

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Fleming.
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

/*
 * Copyright (c) 2005, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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

#ifndef _FS_DEVFS_DEVFS_H_
#define _FS_DEVFS_DEVFS_H_

/* ---------------------------------------------------------------------
 * KERNEL-SPECIFIC DEFINITIONS
 * --------------------------------------------------------------------- */
#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/vnode.h>

#if defined(_KERNEL)
#include <fs/devfs/devfs_pool.h>
#endif /* defined(_KERNEL) */

/* --------------------------------------------------------------------- */

/*
 * Internal representation of a devfs directory entry.
 */
struct devfs_dirent {
	TAILQ_ENTRY(devfs_dirent)	td_entries;

	/* Length of the name stored in this directory entry.  This avoids
	 * the need to recalculate it every time the name is used. */
	uint16_t			td_namelen;

	/* The name of the entry, allocated from a string pool.  This
	* string is not required to be zero-terminated; therefore, the
	* td_namelen field must always be used when accessing its value. */
	char *				td_name;

	/* Pointer to the node this entry refers to. */
	struct devfs_node *		td_node;
};

/* A directory in devfs holds a sorted list of directory entries, which in
 * turn point to other files (which can be directories themselves).
 *
 * In devfs, this list is managed by a tail queue, whose head is defined by
 * the struct devfs_dir type.
 *
 * It is imporant to notice that directories do not have entries for . and
 * .. as other file systems do.  These can be generated when requested
 * based on information available by other means, such as the pointer to
 * the node itself in the former case or the pointer to the parent directory
 * in the latter case.  This is done to simplify devfs's code and, more
 * importantly, to remove redundancy. */
TAILQ_HEAD(devfs_dir, devfs_dirent);

/* Each entry in a directory has a cookie that identifies it.  Cookies
 * supersede offsets within directories because, given how devfs stores
 * directories in memory, there is no such thing as an offset.  (Emulating
 * a real offset could be very difficult.)
 * 
 * The '.', '..' and the end of directory markers have fixed cookies which
 * cannot collide with the cookies generated by other entries.  The cookies
 * fot the other entries are generated based on the memory address on which
 * stores their information is stored.
 *
 * Ideally, using the entry's memory pointer as the cookie would be enough
 * to represent it and it wouldn't cause collisions in any system.
 * Unfortunately, this results in "offsets" with very large values which
 * later raise problems in the Linux compatibility layer (and maybe in other
 * places) as described in PR kern/32034.  Hence we need to workaround this
 * with a rather ugly hack.
 *
 * Linux 32-bit binaries, unless built with _FILE_OFFSET_BITS=64, have off_t
 * set to 'long', which is a 32-bit *signed* long integer.  Regardless of
 * the macro value, GLIBC (2.3 at least) always uses the getdents64
 * system call (when calling readdir) which internally returns off64_t
 * offsets.  In order to make 32-bit binaries work, *GLIBC* converts the
 * 64-bit values returned by the kernel to 32-bit ones and aborts with
 * EOVERFLOW if the conversion results in values that won't fit in 32-bit
 * integers (which it assumes is because the directory is extremely large).
 * This wouldn't cause problems if we were dealing with unsigned integers,
 * but as we have signed integers, this check fails due to sign expansion.
 *
 * For example, consider that the kernel returns the 0xc1234567 cookie to
 * userspace in a off64_t integer.  Later on, GLIBC casts this value to
 * off_t (remember, signed) with code similar to:
 *     system call returns the offset in kernel_value;
 *     off_t casted_value = kernel_value;
 *     if (sizeof(off_t) != sizeof(off64_t) &&
 *         kernel_value != casted_value)
 *             error!
 * In this case, casted_value still has 0xc1234567, but when it is compared
 * for equality against kernel_value, it is promoted to a 64-bit integer and
 * becomes 0xffffffffc1234567, which is different than 0x00000000c1234567.
 * Then, GLIBC assumes this is because the directory is very large.
 *
 * Given that all the above happens in user-space, we have no control over
 * it; therefore we must workaround the issue here.  We do this by
 * truncating the pointer value to a 32-bit integer and hope that there
 * won't be collisions.  In fact, this will not cause any problems in
 * 32-bit platforms but some might arise in 64-bit machines (I'm not sure
 * if they can happen at all in practice).
 *
 * XXX A nicer solution shall be attempted. */
#if defined(_KERNEL)
#define	DEVFS_DIRCOOKIE_DOT	0
#define	DEVFS_DIRCOOKIE_DOTDOT	1
#define	DEVFS_DIRCOOKIE_EOF	2
static __inline
off_t
devfs_dircookie(struct devfs_dirent *de)
{
	off_t cookie;

	cookie = ((off_t)(uintptr_t)de >> 1) & 0x7FFFFFFF;
	KASSERT(cookie != DEVFS_DIRCOOKIE_DOT);
	KASSERT(cookie != DEVFS_DIRCOOKIE_DOTDOT);
	KASSERT(cookie != DEVFS_DIRCOOKIE_EOF);

	return cookie;
}
#endif /* defined(_KERNEL) */

/* --------------------------------------------------------------------- */

/*
 * Internal representation of a devfs file system node.
 *
 * This structure is split in two parts: one holds attributes common
 * to all file types and the other holds data that is only applicable to
 * a particular type.  The code must be careful to only access those
 * attributes that are actually allowed by the node's type.
 */
struct devfs_node {
	/* Doubly-linked list entry which links all existing nodes for a
	 * single file system.  This is provided to ease the removal of
	 * all nodes during the unmount operation. */
	LIST_ENTRY(devfs_node)	tn_entries;

	/* The node's type.  Any of 'VBLK', 'VCHR', 'VDIR', 'VFIFO',
	 * 'VLNK', 'VREG' and 'VSOCK' is allowed.  The usage of vnode
	 * types instead of a custom enumeration is to make things simpler
	 * and faster, as we do not need to convert between two types. */
	enum vtype		tn_type;

	/* Node identifier. */
	ino_t			tn_id;

	/* Node's internal status.  This is used by several file system
	 * operations to do modifications to the node in a delayed
	 * fashion. */
	int			tn_status;
#define	DEVFS_NODE_ACCESSED	(1 << 1)
#define	DEVFS_NODE_MODIFIED	(1 << 2)
#define	DEVFS_NODE_CHANGED	(1 << 3)

	/* The node size.  It does not necessarily match the real amount
	 * of memory consumed by it. */
	off_t			tn_size;

	/* Generic node attributes. */
	uid_t			tn_uid;
	gid_t			tn_gid;
	mode_t			tn_mode;
	int			tn_flags;
	nlink_t			tn_links;
	struct timespec		tn_atime;
	struct timespec		tn_mtime;
	struct timespec		tn_ctime;
	struct timespec		tn_birthtime;
	unsigned long		tn_gen;

	/* Visibility of this node to userland. */
	int			tn_visible;

	/* Head of byte-level lock list (used by devfs_advlock). */
	struct lockf *		tn_lockf;

	/* As there is a single vnode for each active file within the
	 * system, care has to be taken to avoid allocating more than one
	 * vnode per file.  In order to do this, a bidirectional association
	 * is kept between vnodes and nodes.
	 *
	 * Whenever a vnode is allocated, its v_data field is updated to
	 * point to the node it references.  At the same time, the node's
	 * tn_vnode field is modified to point to the new vnode representing
	 * it.  Further attempts to allocate a vnode for this same node will
	 * result in returning a new reference to the value stored in
	 * tn_vnode.
	 *
	 * May be NULL when the node is unused (that is, no vnode has been
	 * allocated for it or it has been reclaimed). */
	kmutex_t		tn_vlock;
	struct vnode *		tn_vnode;

	union {
		/* Valid when tn_type == VBLK || tn_type == VCHR. */
		struct {
			dev_t			tn_rdev;
		} tn_dev;

		/* Valid when tn_type == VDIR. */
		struct {
			/* Pointer to the parent directory.  The root
			 * directory has a pointer to itself in this field;
			 * this property identifies the root node. */
			struct devfs_node *	tn_parent;

			/* Head of a tail-queue that links the contents of
			 * the directory together.  See above for a
			 * description of its contents. */
			struct devfs_dir	tn_dir;

			/* Number and pointer of the first directory entry
			 * returned by the readdir operation if it were
			 * called again to continue reading data from the
			 * same directory as before.  This is used to speed
			 * up reads of long directories, assuming that no
			 * more than one read is in progress at a given time.
			 * Otherwise, these values are discarded and a linear
			 * scan is performed from the beginning up to the
			 * point where readdir starts returning values. */
			off_t			tn_readdir_lastn;
			struct devfs_dirent *	tn_readdir_lastp;
		} tn_dir;

		/* Valid when tn_type == VLNK. */
		struct tn_lnk {
			/* The link's target, allocated from a string pool. */
			char *			tn_link;
		} tn_lnk;

		/* Valid when tn_type == VREG. */
		struct tn_reg {
			/* The contents of regular files stored in a devfs
			 * file system are represented by a single anonymous
			 * memory object (aobj, for short).  The aobj provides
			 * direct access to any position within the file,
			 * because its contents are always mapped in a
			 * contiguous region of virtual memory.  It is a task
			 * of the memory management subsystem (see uvm(9)) to
			 * issue the required page ins or page outs whenever
			 * a position within the file is accessed. */
			struct uvm_object *	tn_aobj;
			size_t			tn_aobj_pages;
		} tn_reg;
	} tn_spec;
};

#if defined(_KERNEL)

LIST_HEAD(devfs_node_list, devfs_node);

/* --------------------------------------------------------------------- */

/*
 * Internal representation of a devfs mount point.
 */
struct devfs_mount {
	/* Maximum number of memory pages available for use by the file
	 * system, set during mount time.  This variable must never be
	 * used directly as it may be bigger than the current amount of
	 * free memory; in the extreme case, it will hold the SIZE_MAX
	 * value.  Instead, use the DEVFS_PAGES_MAX macro. */
	u_int			tm_pages_max;

	/* Number of pages in use by the file system.  Cannot be bigger
	 * than the value returned by DEVFS_PAGES_MAX in any case. */
	u_int			tm_pages_used;

	/* Pointer to the node representing the root directory of this
	 * file system. */
	struct devfs_node *	tm_root;

	/* Maximum number of possible nodes for this file system; set
	 * during mount time.  We need a hard limit on the maximum number
	 * of nodes to avoid allocating too much of them; their objects
	 * cannot be released until the file system is unmounted.
	 * Otherwise, we could easily run out of memory by creating lots
	 * of empty files and then simply removing them. */
	u_int			tm_nodes_max;

	/* Number of nodes currently allocated.  This number only grows.
	 * When it reaches tm_nodes_max, no more new nodes can be allocated.
	 * Of course, the old, unused ones can be reused. */
	u_int			tm_nodes_cnt;

	/* Node list. */
	kmutex_t		tm_lock;
	struct devfs_node_list	tm_nodes;

	/* Pools used to store file system meta data.  These are not shared
	 * across several instances of devfs for the reasons described in
	 * devfs_pool.c. */
	struct devfs_pool	tm_dirent_pool;
	struct devfs_pool	tm_node_pool;
	struct devfs_str_pool	tm_str_pool;

	/* Default flag for the visibility of nodes. */
	int			tm_visible;

	/* Were we mounted by init(8)? */
	int			tm_init;
};

/* --------------------------------------------------------------------- */

/*
 * This structure maps a file identifier to a devfs node.  Used by the
 * NFS code.
 */
struct devfs_fid {
	uint16_t		tf_len;
	uint16_t		tf_pad;
	uint32_t		tf_gen;
	ino_t			tf_id;
};

/* --------------------------------------------------------------------- */

/*
 * Prototypes for devfs_subr.c.
 */

int	devfs_alloc_node(struct devfs_mount *, enum vtype,
	    uid_t uid, gid_t gid, mode_t mode, struct devfs_node *,
	    char *, dev_t, struct devfs_node **);
void	devfs_free_node(struct devfs_mount *, struct devfs_node *);
int	devfs_alloc_dirent(struct devfs_mount *, struct devfs_node *,
	    const char *, uint16_t, struct devfs_dirent **);
void	devfs_free_dirent(struct devfs_mount *, struct devfs_dirent *,
	    bool);
int	devfs_alloc_vp(struct mount *, struct devfs_node *, struct vnode **);
void	devfs_free_vp(struct vnode *);
int	devfs_alloc_file(struct vnode *, struct vnode **, struct vattr *,
	    struct componentname *, char *);
void	devfs_dir_attach(struct vnode *, struct devfs_dirent *);
void	devfs_dir_detach(struct vnode *, struct devfs_dirent *);
struct devfs_dirent *	devfs_dir_lookup(struct devfs_node *node,
			    struct componentname *cnp);
int	devfs_dir_getdotdent(struct devfs_node *, struct uio *);
int	devfs_dir_getdotdotdent(struct devfs_node *, struct uio *);
struct devfs_dirent *	devfs_dir_lookupbycookie(struct devfs_node *, off_t);
int	devfs_dir_getdents(struct devfs_node *, struct uio *, off_t *);
int	devfs_reg_resize(struct vnode *, off_t);
size_t	devfs_mem_info(bool);
int	devfs_chflags(struct vnode *, int, kauth_cred_t, struct lwp *);
int	devfs_chmod(struct vnode *, mode_t, kauth_cred_t, struct lwp *);
int	devfs_chown(struct vnode *, uid_t, gid_t, kauth_cred_t, struct lwp *);
int	devfs_chsize(struct vnode *, u_quad_t, kauth_cred_t, struct lwp *);
int	devfs_chtimes(struct vnode *, struct timespec *, struct timespec *,
	    int, kauth_cred_t, struct lwp *);
void	devfs_itimes(struct vnode *, const struct timespec *,
	    const struct timespec *);

void	devfs_update(struct vnode *, const struct timespec *,
	    const struct timespec *, int);
int	devfs_truncate(struct vnode *, off_t);

int	devfs_init_nodes(struct devfs_mount *, struct mount *, int);
int	devfs_new_node(struct devfs_mount *, struct mount *, struct vnode *,
	    dev_t, char *);

/* --------------------------------------------------------------------- */

/*
 * Convenience macros to simplify some logical expressions.
 */
#define IMPLIES(a, b) (!(a) || (b))
#define IFF(a, b) (IMPLIES(a, b) && IMPLIES(b, a))

/* --------------------------------------------------------------------- */

/*
 * Checks that the directory entry pointed by 'de' matches the name 'name'
 * with a length of 'len'.
 */
#define DEVFS_DIRENT_MATCHES(de, name, len) \
    (de->td_namelen == (uint16_t)len && \
    memcmp((de)->td_name, (name), (de)->td_namelen) == 0)

/* --------------------------------------------------------------------- */

/*
 * Ensures that the node pointed by 'node' is a directory and that its
 * contents are consistent with respect to directories.
 */
#define DEVFS_VALIDATE_DIR(node) \
    KASSERT((node)->tn_type == VDIR); \
    KASSERT((node)->tn_size % sizeof(struct devfs_dirent) == 0); \
    KASSERT((node)->tn_spec.tn_dir.tn_readdir_lastp == NULL || \
        devfs_dircookie((node)->tn_spec.tn_dir.tn_readdir_lastp) == \
        (node)->tn_spec.tn_dir.tn_readdir_lastn);

#define DEVFS_VISIBLE_NODE(node) (node->tn_visible == 1)
    
/* --------------------------------------------------------------------- */

/*
 * Memory management stuff.
 */

/* Amount of memory pages to reserve for the system (e.g., to not use by
 * devfs).
 * XXX: Should this be tunable through sysctl, for instance? */
#define DEVFS_PAGES_RESERVED (4 * 1024 * 1024 / PAGE_SIZE)

/* Returns the maximum size allowed for a devfs file system.  This macro
 * must be used instead of directly retrieving the value from tm_pages_max.
 * The reason is that the size of a devfs file system is dynamic: it lets
 * the user store files as long as there is enough free memory (including
 * physical memory and swap space).  Therefore, the amount of memory to be
 * used is either the limit imposed by the user during mount time or the
 * amount of available memory, whichever is lower.  To avoid consuming all
 * the memory for a given mount point, the system will always reserve a
 * minimum of DEVFS_PAGES_RESERVED pages, which is also taken into account
 * by this macro (see above). */
static __inline size_t
DEVFS_PAGES_MAX(struct devfs_mount *tmp)
{
	size_t freepages;

	freepages = devfs_mem_info(false);
	if (freepages < DEVFS_PAGES_RESERVED)
		freepages = 0;
	else
		freepages -= DEVFS_PAGES_RESERVED;

	return MIN(tmp->tm_pages_max, freepages + tmp->tm_pages_used);
}

/* Returns the available space for the given file system. */
#define DEVFS_PAGES_AVAIL(tmp)		\
    ((ssize_t)(DEVFS_PAGES_MAX(tmp) - (tmp)->tm_pages_used))

/* --------------------------------------------------------------------- */

/*
 * Macros/functions to convert from generic data structures to devfs
 * specific ones.
 */

static __inline
struct devfs_mount *
VFS_TO_DEVFS(struct mount *mp)
{
	struct devfs_mount *tmp;

#ifdef KASSERT
	KASSERT((mp) != NULL && (mp)->mnt_data != NULL);
#endif
	tmp = (struct devfs_mount *)(mp)->mnt_data;
	return tmp;
}

#endif /* defined(_KERNEL) */

static __inline
struct devfs_node *
VP_TO_DEVFS_NODE(struct vnode *vp)
{
	struct devfs_node *node;

#ifdef KASSERT
	KASSERT((vp) != NULL && (vp)->v_data != NULL);
#endif
	node = (struct devfs_node *)vp->v_data;
	return node;
}

#if defined(_KERNEL)

static __inline
struct devfs_node *
VP_TO_DEVFS_DIR(struct vnode *vp)
{
	struct devfs_node *node;

	node = VP_TO_DEVFS_NODE(vp);
#ifdef KASSERT
	DEVFS_VALIDATE_DIR(node);
#endif
	return node;
}

#endif /* defined(_KERNEL) */

/* ---------------------------------------------------------------------
 * USER AND KERNEL DEFINITIONS
 * --------------------------------------------------------------------- */

/*
 * This structure is used to communicate mount parameters between userland
 * and kernel space.
 */
#define DEVFS_ARGS_VERSION	1
struct devfs_args {
	int			ta_version;

	/* Size counters. */
	off_t			ta_size_max;

	/* Root node attributes. */
	uid_t			ta_root_uid;
	gid_t			ta_root_gid;
	mode_t			ta_root_mode;

	/* Visibility for nodes. */
	int			ta_visible;

	/* Are we being called by init(8)? */
	int			ta_init;
};
#endif /* _FS_DEVFS_DEVFS_H_ */

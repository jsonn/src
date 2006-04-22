/*	$NetBSD: tmpfs.h,v 1.13.6.1 2006/04/22 11:39:58 simonb Exp $	*/

/*
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
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

#ifndef _FS_TMPFS_TMPFS_H_
#define _FS_TMPFS_TMPFS_H_

/* ---------------------------------------------------------------------
 * KERNEL-SPECIFIC DEFINITIONS
 * --------------------------------------------------------------------- */
#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/vnode.h>

#include <fs/tmpfs/tmpfs_pool.h>

/* --------------------------------------------------------------------- */

/*
 * Internal representation of a tmpfs directory entry.
 */
struct tmpfs_dirent {
	TAILQ_ENTRY(tmpfs_dirent)	td_entries;

	/* Length of the name stored in this directory entry.  This avoids
	 * the need to recalculate it every time the name is used. */
	uint16_t			td_namelen;

	/* The name of the entry, allocated from a string pool.  This
	* string is not required to be zero-terminated; therefore, the
	* td_namelen field must always be used when accessing its value. */
	char *				td_name;

	/* Pointer to the node this entry refers to. */
	struct tmpfs_node *		td_node;
};

/* A directory in tmpfs holds a sorted list of directory entries, which in
 * turn point to other files (which can be directories themselves).
 *
 * In tmpfs, this list is managed by a tail queue, whose head is defined by
 * the struct tmpfs_dir type.
 *
 * It is imporant to notice that directories do not have entries for . and
 * .. as other file systems do.  These can be generated when requested
 * based on information available by other means, such as the pointer to
 * the node itself in the former case or the pointer to the parent directory
 * in the latter case.  This is done to simplify tmpfs's code and, more
 * importantly, to remove redundancy. */
TAILQ_HEAD(tmpfs_dir, tmpfs_dirent);

#define	TMPFS_DIRCOOKIE(dirent)	((off_t)(uintptr_t)(dirent))
#define	TMPFS_DIRCOOKIE_DOT	0
#define	TMPFS_DIRCOOKIE_DOTDOT	1
#define	TMPFS_DIRCOOKIE_EOF	2

/* --------------------------------------------------------------------- */

/*
 * Internal representation of a tmpfs file system node.
 *
 * This structure is splitted in two parts: one holds attributes common
 * to all file types and the other holds data that is only applicable to
 * a particular type.  The code must be careful to only access those
 * attributes that are actually allowed by the node's type.
 */
struct tmpfs_node {
	/* Doubly-linked list entry which links all existing nodes for a
	 * single file system.  This is provided to ease the removal of
	 * all nodes during the unmount operation. */
	LIST_ENTRY(tmpfs_node)	tn_entries;

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
#define	TMPFS_NODE_ACCESSED	(1 << 1)
#define	TMPFS_NODE_MODIFIED	(1 << 2)
#define	TMPFS_NODE_CHANGED	(1 << 3)

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

	/* Head of byte-level lock list (used by tmpfs_advlock). */
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
	struct vnode *		tn_vnode;

	/* Pointer to the node returned by tmpfs_lookup() after doing a
	 * delete or a rename lookup; its value is only valid in these two
	 * situations.  In case we were looking up . or .., it holds a null
	 * pointer. */
	struct tmpfs_dirent *	tn_lookup_dirent;

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
			struct tmpfs_node *	tn_parent;

			/* Head of a tail-queue that links the contents of
			 * the directory together.  See above for a
			 * description of its contents. */
			struct tmpfs_dir	tn_dir;

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
			struct tmpfs_dirent *	tn_readdir_lastp;
		} tn_dir;

		/* Valid when tn_type == VLNK. */
		struct tn_lnk {
			/* The link's target, allocated from a string pool. */
			char *			tn_link;
		} tn_lnk;

		/* Valid when tn_type == VREG. */
		struct tn_reg {
			/* The contents of regular files stored in a tmpfs
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
LIST_HEAD(tmpfs_node_list, tmpfs_node);

/* --------------------------------------------------------------------- */

/*
 * Internal representation of a tmpfs mount point.
 */
struct tmpfs_mount {
	/* Maximum number of memory pages available for use by the file
	 * system, set during mount time.  This variable must never be
	 * used directly as it may be bigger that the current amount of
	 * free memory; in the extreme case, it will hold the SIZE_MAX
	 * value.  Instead, use the TMPFS_PAGES_MAX macro. */
	size_t			tm_pages_max;

	/* Number of pages in use by the file system.  Cannot be bigger
	 * than the value returned by TMPFS_PAGES_MAX in any case. */
	size_t			tm_pages_used;

	/* Pointer to the node representing the root directory of this
	 * file system. */
	struct tmpfs_node *	tm_root;

	/* Maximum number of possible nodes for this file system; set
	 * during mount time.  We need a hard limit on the maximum number
	 * of nodes to avoid allocating too much of them; their objects
	 * cannot be released until the file system is unmounted.
	 * Otherwise, we could easily run out of memory by creating lots
	 * of empty files and then simply removing them. */
	ino_t			tm_nodes_max;

	/* Number of nodes currently allocated.  This number only grows.
	 * When it reaches tm_nodes_max, no more new nodes can be allocated.
	 * Of course, the old, unused ones can be reused. */
	ino_t			tm_nodes_last;

	/* Nodes are organized in two different lists.  The used list
	 * contains all nodes that are currently used by the file system;
	 * i.e., they refer to existing files.  The available list contains
	 * all nodes that are currently available for use by new files.
	 * Nodes must be kept in this list (instead of deleting them)
	 * because we need to keep track of their generation number (tn_gen
	 * field).
	 *
	 * Note that nodes are lazily allocated: if the available list is
	 * empty and we have enough space to create more nodes, they will be
	 * created and inserted in the used list.  Once these are released,
	 * they will go into the available list, remaining alive until the
	 * file system is unmounted. */
	struct tmpfs_node_list	tm_nodes_used;
	struct tmpfs_node_list	tm_nodes_avail;

	/* Pools used to store file system meta data.  These are not shared
	 * across several instances of tmpfs for the reasons described in
	 * tmpfs_pool.c. */
	struct tmpfs_pool	tm_dirent_pool;
	struct tmpfs_pool	tm_node_pool;
	struct tmpfs_str_pool	tm_str_pool;
};

/* --------------------------------------------------------------------- */

/*
 * This structure maps a file identifier to a tmpfs node.  Used by the
 * NFS code.
 */
struct tmpfs_fid {
	uint16_t		tf_len;
	uint16_t		tf_pad;
	uint32_t		tf_gen;
	ino_t			tf_id;
};

/* --------------------------------------------------------------------- */

#ifdef _KERNEL
/*
 * Prototypes for tmpfs_subr.c.
 */

int	tmpfs_alloc_node(struct tmpfs_mount *, enum vtype,
	    uid_t uid, gid_t gid, mode_t mode, struct tmpfs_node *,
	    char *, dev_t, struct proc *, struct tmpfs_node **);
void	tmpfs_free_node(struct tmpfs_mount *, struct tmpfs_node *);
int	tmpfs_alloc_dirent(struct tmpfs_mount *, struct tmpfs_node *,
	    const char *, uint16_t, struct tmpfs_dirent **);
void	tmpfs_free_dirent(struct tmpfs_mount *, struct tmpfs_dirent *,
	    boolean_t);
int	tmpfs_alloc_vp(struct mount *, struct tmpfs_node *, struct vnode **);
void	tmpfs_free_vp(struct vnode *);
int	tmpfs_alloc_file(struct vnode *, struct vnode **, struct vattr *,
	    struct componentname *, char *);
void	tmpfs_dir_attach(struct vnode *, struct tmpfs_dirent *);
void	tmpfs_dir_detach(struct vnode *, struct tmpfs_dirent *);
struct tmpfs_dirent *	tmpfs_dir_lookup(struct tmpfs_node *node,
			    struct componentname *cnp);
int	tmpfs_dir_getdotdent(struct tmpfs_node *, struct uio *);
int	tmpfs_dir_getdotdotdent(struct tmpfs_node *, struct uio *);
struct tmpfs_dirent *	tmpfs_dir_lookupbycookie(struct tmpfs_node *, off_t);
int	tmpfs_dir_getdents(struct tmpfs_node *, struct uio *, off_t *);
int	tmpfs_reg_resize(struct vnode *, off_t);
size_t	tmpfs_mem_info(boolean_t);
int	tmpfs_chflags(struct vnode *, int, struct ucred *, struct proc *);
int	tmpfs_chmod(struct vnode *, mode_t, struct ucred *, struct proc *);
int	tmpfs_chown(struct vnode *, uid_t, gid_t, struct ucred *,
	    struct proc *);
int	tmpfs_chsize(struct vnode *, u_quad_t, struct ucred *, struct proc *);
int	tmpfs_chtimes(struct vnode *, struct timespec *, struct timespec *,
	    int, struct ucred *, struct lwp *);
void	tmpfs_itimes(struct vnode *, const struct timespec *,
	    const struct timespec *);

void	tmpfs_update(struct vnode *, const struct timespec *,
	    const struct timespec *, int);
int	tmpfs_truncate(struct vnode *, off_t);

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
#define TMPFS_DIRENT_MATCHES(de, name, len) \
    (de->td_namelen == (uint16_t)len && \
    memcmp((de)->td_name, (name), (de)->td_namelen) == 0)

/* --------------------------------------------------------------------- */

/*
 * Ensures that the node pointed by 'node' is a directory and that its
 * contents are consistent with respect to directories.
 */
#define TMPFS_VALIDATE_DIR(node) \
    KASSERT((node)->tn_type == VDIR); \
    KASSERT((node)->tn_size % sizeof(struct tmpfs_dirent) == 0); \
    KASSERT((node)->tn_spec.tn_dir.tn_readdir_lastp == NULL || \
        TMPFS_DIRCOOKIE((node)->tn_spec.tn_dir.tn_readdir_lastp) == \
        (node)->tn_spec.tn_dir.tn_readdir_lastn);

/* --------------------------------------------------------------------- */

/*
 * Memory management stuff.
 */

/* Amount of memory pages to reserve for the system (e.g., to not use by
 * tmpfs).
 * XXX: Should this be tunable through sysctl, for instance? */
#define TMPFS_PAGES_RESERVED (4 * 1024 * 1024 / PAGE_SIZE)

/* Returns the maximum size allowed for a tmpfs file system.  This macro
 * must be used instead of directly retrieving the value from tm_pages_max.
 * The reason is that the size of a tmpfs file system is dynamic: it lets
 * the user store files as long as there is enough free memory (including
 * physical memory and swap space).  Therefore, the amount of memory to be
 * used is either the limit imposed by the user during mount time or the
 * amount of available memory, whichever is lower.  To avoid consuming all
 * the memory for a given mount point, the system will always reserve a
 * minimum of TMPFS_PAGES_RESERVED pages, which is also taken into account
 * by this macro (see above). */
static __inline size_t
TMPFS_PAGES_MAX(struct tmpfs_mount *tmp)
{
	size_t freepages;

	freepages = tmpfs_mem_info(FALSE);
	if (freepages < TMPFS_PAGES_RESERVED)
		freepages = 0;
	else
		freepages -= TMPFS_PAGES_RESERVED;

	return MIN(tmp->tm_pages_max, freepages + tmp->tm_pages_used);
}

/* Returns the available space for the given file system. */
#define TMPFS_PAGES_AVAIL(tmp) (TMPFS_PAGES_MAX(tmp) - (tmp)->tm_pages_used)

#endif

/* --------------------------------------------------------------------- */

/*
 * Macros/functions to convert from generic data structures to tmpfs
 * specific ones.
 */

static __inline
struct tmpfs_mount *
VFS_TO_TMPFS(struct mount *mp)
{
	struct tmpfs_mount *tmp;

#ifdef KASSERT
	KASSERT((mp) != NULL && (mp)->mnt_data != NULL);
#endif
	tmp = (struct tmpfs_mount *)(mp)->mnt_data;
	return tmp;
}

static __inline
struct tmpfs_node *
VP_TO_TMPFS_NODE(struct vnode *vp)
{
	struct tmpfs_node *node;

#ifdef KASSERT
	KASSERT((vp) != NULL && (vp)->v_data != NULL);
#endif
	node = (struct tmpfs_node *)vp->v_data;
	return node;
}

static __inline
struct tmpfs_node *
VP_TO_TMPFS_DIR(struct vnode *vp)
{
	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);
#ifdef KASSERT
	TMPFS_VALIDATE_DIR(node);
#endif
	return node;
}

/* ---------------------------------------------------------------------
 * USER AND KERNEL DEFINITIONS
 * --------------------------------------------------------------------- */

/*
 * This structure is used to communicate mount parameters between userland
 * and kernel space.
 */
#define TMPFS_ARGS_VERSION	1
struct tmpfs_args {
	int			ta_version;

	/* Size counters. */
	ino_t			ta_nodes_max;
	off_t			ta_size_max;

	/* Root node attributes. */
	uid_t			ta_root_uid;
	gid_t			ta_root_gid;
	mode_t			ta_root_mode;
};
#endif /* _FS_TMPFS_TMPFS_H_ */

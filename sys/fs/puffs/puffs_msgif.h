/*	$NetBSD: puffs_msgif.h,v 1.42.2.1 2007/08/15 13:48:59 skrll Exp $	*/

/*
 * Copyright (c) 2005, 2006  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _PUFFS_MSGIF_H_
#define _PUFFS_MSGIF_H_

#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioccom.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/ucred.h>
#include <sys/statvfs.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>

#include <uvm/uvm_prot.h>

#define PUFFSOP_VFS	1
#define PUFFSOP_VN	2
#define PUFFSOP_CACHE	3
#define PUFFSOPFLAG_FAF	0x10	/* fire-and-forget */

#define PUFFSOP_OPCMASK		0x03
#define PUFFSOP_OPCLASS(a)	((a) & PUFFSOP_OPCMASK)
#define PUFFSOP_WANTREPLY(a)	(((a) & PUFFSOPFLAG_FAF) == 0)

/* XXX: we don't need everything */
enum {
	PUFFS_VFS_MOUNT,	PUFFS_VFS_START,	PUFFS_VFS_UNMOUNT,
	PUFFS_VFS_ROOT,		PUFFS_VFS_STATVFS,	PUFFS_VFS_SYNC,
	PUFFS_VFS_VGET,		PUFFS_VFS_FHTOVP,	PUFFS_VFS_VPTOFH,
	PUFFS_VFS_INIT,		PUFFS_VFS_DONE,		PUFFS_VFS_SNAPSHOT,
	PUFFS_VFS_EXTATTCTL,	PUFFS_VFS_SUSPEND
};
#define PUFFS_VFS_MAX PUFFS_VFS_EXTATTCTL

/* moreXXX: we don't need everything here either */
enum {
	PUFFS_VN_LOOKUP,	PUFFS_VN_CREATE,	PUFFS_VN_MKNOD,
	PUFFS_VN_OPEN,		PUFFS_VN_CLOSE,		PUFFS_VN_ACCESS,
	PUFFS_VN_GETATTR,	PUFFS_VN_SETATTR,	PUFFS_VN_READ,
	PUFFS_VN_WRITE,		PUFFS_VN_IOCTL,		PUFFS_VN_FCNTL,
	PUFFS_VN_POLL,		PUFFS_VN_KQFILTER,	PUFFS_VN_REVOKE,
	PUFFS_VN_MMAP,		PUFFS_VN_FSYNC,		PUFFS_VN_SEEK,
	PUFFS_VN_REMOVE,	PUFFS_VN_LINK,		PUFFS_VN_RENAME,
	PUFFS_VN_MKDIR,		PUFFS_VN_RMDIR,		PUFFS_VN_SYMLINK,
	PUFFS_VN_READDIR,	PUFFS_VN_READLINK,	PUFFS_VN_ABORTOP,
	PUFFS_VN_INACTIVE,	PUFFS_VN_RECLAIM,	PUFFS_VN_LOCK,
	PUFFS_VN_UNLOCK,	PUFFS_VN_BMAP,		PUFFS_VN_STRATEGY,
	PUFFS_VN_PRINT,		PUFFS_VN_ISLOCKED,	PUFFS_VN_PATHCONF,
	PUFFS_VN_ADVLOCK,	PUFFS_VN_LEASE,		PUFFS_VN_WHITEOUT,
	PUFFS_VN_GETPAGES,	PUFFS_VN_PUTPAGES,	PUFFS_VN_GETEXTATTR,
	PUFFS_VN_LISTEXTATTR,	PUFFS_VN_OPENEXTATTR,	PUFFS_VN_DELETEEXTATTR,
	PUFFS_VN_SETEXTATTR
};
#define PUFFS_VN_MAX PUFFS_VN_SETEXTATTR

#define PUFFSDEVELVERS	0x80000000
#define PUFFSVERSION	16
#define PUFFSNAMESIZE	32

#define PUFFS_TYPEPREFIX "puffs|"

#define PUFFS_TYPELEN (_VFS_NAMELEN - (sizeof(PUFFS_TYPEPREFIX)+1))
#define PUFFS_NAMELEN (_VFS_MNAMELEN-1)

struct puffs_kargs {
	unsigned int	pa_vers;
	int		pa_fd;
	uint32_t	pa_flags;

	size_t		pa_maxreqlen;
	int		pa_nhashbuckets;

	size_t		pa_fhsize;
	int		pa_fhflags;

	void		*pa_root_cookie;
	enum vtype	pa_root_vtype;
	voff_t		pa_root_vsize;
	dev_t		pa_root_rdev;

	struct statvfs	pa_svfsb;
	
	char		pa_typename[_VFS_NAMELEN];
	char		pa_mntfromname[_VFS_MNAMELEN];

	uint8_t		pa_vnopmask[PUFFS_VN_MAX];
};
#define PUFFS_KFLAG_NOCACHE_NAME	0x01	/* don't use name cache     */
#define PUFFS_KFLAG_NOCACHE_PAGE	0x02	/* don't use page cache	    */
#define PUFFS_KFLAG_NOCACHE		0x03	/* no cache whatsoever      */
#define PUFFS_KFLAG_ALLOPS		0x04	/* ignore pa_vnopmask       */
#define PUFFS_KFLAG_WTCACHE		0x08	/* write-through page cache */
#define PUFFS_KFLAG_IAONDEMAND		0x10	/* inactive only on demand  */
#define PUFFS_KFLAG_LOOKUP_FULLPNBUF	0x20	/* full pnbuf in lookup     */
#define PUFFS_KFLAG_MASK		0x3f

#define PUFFS_FHFLAG_DYNAMIC	0x01
#define PUFFS_FHFLAG_NFSV2	0x02
#define PUFFS_FHFLAG_NFSV3	0x04
#define PUFFS_FHFLAG_PROTOMASK	0x06

#define PUFFS_FHSIZE_MAX	1020	/* XXX: FHANDLE_SIZE_MAX - 4 */

/*
 * This is the device minor number for the cloning device.  Make it
 * a high number "just in case", even though we don't want to open
 * any specific devices currently.
 */
#define PUFFS_CLONER 0x7ffff

struct puffs_reqh_get {
	void	*phg_buf;	/* user buffer		*/
	size_t	phg_buflen;	/* user buffer length	*/

	int	phg_nops;	/* max ops user wants / number delivered */
	int	phg_more;	/* advisory: more ops available? */
};

struct puffs_reqh_put {
	int		php_nops;	/* ops available / ops handled */

	/* these describe the first request */
	uint64_t	php_id;		/* request id */
	void		*php_buf;	/* user buffer address */
	size_t		php_buflen;	/* user buffer length, hdr NOT incl. */
};

/*
 * The requests work as follows:
 *
 *  + GETOP: When fetching operations from the kernel, the user server
 *           supplies a flat buffer into which operations are written.
 *           The number of operations written to the buffer is
 *           MIN(prhg_nops, requests waiting, space in buffer).
 *           
 *           Operations follow each other and each one is described
 *           by the puffs_req structure, which is immediately followed
 *           by the aligned request data buffer in preq_buf.  The next
 *           puffs_req can be found at preq + preq_buflen.
 *
 *           Visually, the server should expect:
 *
 *           |<-- preq_buflen -->|
 *           ---------------------------------------------------------------
 *           |hdr|  buffer |align|hdr| buffer |hdr| ....   |buffer|        |
 *           ---------------------------------------------------------------
 *           ^ start       ^ unaligned        ^ aligned    ^ always aligned
 *
 *           The server is allowed to modify the contents of the buffers.
 *           Since the headers are the same size for both get and put, it
 *           is possible to handle all operations simply by doing in-place
 *           modification.  Where more input is expected that what was put
 *           out, the kernel leaves a hole in the buffer.  This hole size
 *           is derived from the operational semantics known by the vnode
 *           layer.  The hole size is included in preq_buflen.  The
 *           amount of relevant information in the buffer is call-specific
 *           and can be deduced by the server from the call type.
 *
 *  + PUTOP: When returning the results of an operation to the kernel, the
 *           user server is allowed to input results in a scatter-gather
 *           fashion.  Each request is made up of a header and the buffer.
 *           The header describes the *NEXT* request for copyin, *NOT* the
 *           current one.  The first request is described in puffs_reqh_put
 *           and the last one is left uninterpreted.  This is to halve the
 *           amount of copyin's required.
 *
 *           Fans of my ascii art, rejoice:
 *               /-------------------->| preq_buflen     |
 *           ---^----------------------------------------------------------
 *           |hdr|buffer| empty spaces |hdr|    buffer   |                |
 *           --v-----------------------^-v---------------------------------
 *             \------- preq_buf -----/  \------- preq_buf ....
 *
 *           This scheme also allows for better in-place modification of the
 *           request buffer when handling requests.  The vision is that
 *           operations which can be immediately satisfied will be edited
 *           in-place while ones which can't will be left blank.  Also,
 *           requests from async operations which have been satisfied
 *           meanwhile can be included.
 *
 *           The total number of operations is given by the ioctl control
 *           structure puffs_reqh.  The values in the header in the final
 *           are not used.
 */
struct puffs_req {
	uint64_t	preq_id;		/* get: cur, put: next */

	union u {
		struct {
			uint8_t	opclass;	/* cur */
			uint8_t	optype;		/* cur */

			/*
			 * preq_cookie is the node cookie associated with
			 * the request.  It always maps 1:1 to a vnode
			 * and could map to a userspace struct puffs_node.
			 * The cookie usually describes the first
			 * vnode argument of the VOP_POP() in question.
			 */

			void	*cookie;	/* cur */
		} out;
		struct {
			int	rv;		/* cur */
			int	setbacks;	/* cur */
			void	*buf;		/* next */
		} in;
	} u;

	size_t  preq_buflen;			/* get: cur, put: next */
	/*
	 * the following helper pads the struct size to md alignment
	 * multiple (should size_t not cut it).  it makes sure that
	 * whatever comes after this struct is aligned
	 */
	uint8_t	preq_buf[0] __aligned(ALIGNBYTES+1);
};
#define preq_opclass	u.out.opclass
#define preq_optype	u.out.optype
#define preq_cookie	u.out.cookie
#define preq_rv		u.in.rv
#define preq_setbacks	u.in.setbacks
#define preq_nextbuf	u.in.buf

#define PUFFS_SETBACK_INACT_N1	0x01	/* set VOP_INACTIVE for node 1 */
#define PUFFS_SETBACK_INACT_N2	0x02	/* set VOP_INACTIVE for node 2 */
#define PUFFS_SETBACK_NOREF_N1	0x04	/* set pn PN_NOREFS for node 1 */
#define PUFFS_SETBACK_NOREF_N2	0x08	/* set pn PN_NOREFS for node 2 */
#define PUFFS_SETBACK_MASK	0x0f

/*
 * Some operations have unknown size requirements.  So as the first
 * stab at handling it, do an extra bounce between the kernel and
 * userspace.
 */
struct puffs_sizeop {
	uint64_t	pso_reqid;

	uint8_t		*pso_userbuf;
	size_t		pso_bufsize;
};

/*
 * Flush operation.  This can be used to invalidate:
 * 1) name cache for one node
 * 2) name cache for all children 
 * 3) name cache for the entire mount
 * 4) page cache for a set of ranges in one node
 * 5) page cache for one entire node
 *
 * It can be used to flush:
 * 1) page cache for a set of ranges in one node
 * 2) page cache for one entire node
 */

/* XXX: needs restructuring */
struct puffs_flush {
	void		*pf_cookie;

	int		pf_op;
	off_t		pf_start;
	off_t		pf_end;
};
#define PUFFS_INVAL_NAMECACHE_NODE		0
#define PUFFS_INVAL_NAMECACHE_DIR		1
#define PUFFS_INVAL_NAMECACHE_ALL		2
#define PUFFS_INVAL_PAGECACHE_NODE_RANGE	3
#define PUFFS_FLUSH_PAGECACHE_NODE_RANGE	4


/*
 * Available ioctl operations
 */
#define PUFFSGETOP		_IOWR('p', 1, struct puffs_reqh_get)
#define PUFFSPUTOP		_IOWR('p', 2, struct puffs_reqh_put)
#define PUFFSSIZEOP		_IOWR('p', 3, struct puffs_sizeop)
#define PUFFSFLUSHOP		_IOW ('p', 4, struct puffs_flush)
#if 0
#define PUFFSFLUSHMULTIOP	_IOW ('p', 5, struct puffs_flushmulti)
#endif
#define PUFFSSUSPENDOP		_IO  ('p', 6)
#define PUFFSREQSIZEOP		_IOR ('p', 7, size_t)


/*
 * Credentials for an operation.  Can be either struct uucred for
 * ops called from a credential context or NOCRED/FSCRED for ops
 * called from within the kernel.  It is up to the implementation
 * if it makes a difference between these two and the super-user.
 */
struct puffs_kcred {
	struct uucred	pkcr_uuc;
	uint8_t		pkcr_type;
	uint8_t		pkcr_internal;
};
#define PUFFCRED_TYPE_UUC	1
#define PUFFCRED_TYPE_INTERNAL	2
#define PUFFCRED_CRED_NOCRED	1
#define PUFFCRED_CRED_FSCRED	2

/*
 * 2*MAXPHYS is the max size the system will attempt to copy,
 * else treated as garbage
 */
#define PUFFS_REQ_MAXSIZE	2*MAXPHYS
#define PUFFS_REQSTRUCT_MAX	4096 /* XXX: approxkludge */

#define PUFFS_TOMOVE(a,b) (MIN((a), b->pmp_req_maxsize - PUFFS_REQSTRUCT_MAX))

/* caller id */
struct puffs_kcid {
	int	pkcid_type;
	pid_t	pkcid_pid;
	lwpid_t	pkcid_lwpid;
};
#define PUFFCID_TYPE_REAL	1
#define PUFFCID_TYPE_FAKE	2

/* puffs struct componentname built by kernel */
struct puffs_kcn {
	/* args */
	u_long			pkcn_nameiop;	/* namei operation	*/
	u_long			pkcn_flags;	/* flags		*/

	char pkcn_name[MAXPATHLEN];	/* nulterminated path component */
	long pkcn_namelen;		/* current component length	*/
	long pkcn_consume;		/* IN: extra chars server ate   */
};

/*
 * XXX: figure out what to do with these, copied from namei.h for now
 */
#define	PUFFSLOOKUP_LOOKUP	0	/* perform name lookup only */
#define PUFFSLOOKUP_CREATE	1	/* setup for file creation */
#define PUFFSLOOKUP_DELETE	2	/* setup for file deletion */
#define PUFFSLOOKUP_RENAME	3	/* setup for file renaming */
#define PUFFSLOOKUP_OPMASK	3	/* mask for operation */

#define PUFFSLOOKUP_FOLLOW	0x00004	/* follow final symlink */
#define PUFFSLOOKUP_NOFOLLOW	0x00008	/* don't follow final symlink */
#define PUFFSLOOKUP_ISLASTCN	0x08000 /* is last component of lookup */
#define PUFFSLOOKUP_REQUIREDIR	0x80000 /* must be directory */


/*
 * Next come the individual requests.  They are all subclassed from
 * puffs_req and contain request-specific fields in addition.  Note
 * that there are some requests which have to handle arbitrary-length
 * buffers.
 *
 * The division is the following: puffs_req is to be touched only
 * by generic routines while the other stuff is supposed to be
 * modified only by specific routines.
 */

/*
 * aux structures for vfs operations.
 */
struct puffs_vfsreq_unmount {
	struct puffs_req	pvfsr_pr;

	int			pvfsr_flags;
	struct puffs_kcid	pvfsr_cid;
};

struct puffs_vfsreq_statvfs {
	struct puffs_req	pvfsr_pr;

	struct statvfs		pvfsr_sb;
	struct puffs_kcid	pvfsr_cid;
};

struct puffs_vfsreq_sync {
	struct puffs_req	pvfsr_pr;

	struct puffs_kcred	pvfsr_cred;
	struct puffs_kcid	pvfsr_cid;
	int			pvfsr_waitfor;
};

struct puffs_vfsreq_fhtonode {
	struct puffs_req	pvfsr_pr;

	void			*pvfsr_fhcookie;	/* IN	*/
	enum vtype		pvfsr_vtype;		/* IN	*/
	voff_t			pvfsr_size;		/* IN	*/
	dev_t			pvfsr_rdev;		/* IN	*/

	size_t			pvfsr_dsize;		/* OUT */
	uint8_t			pvfsr_data[0]		/* OUT, XXX */
				    __aligned(ALIGNBYTES+1);
};

struct puffs_vfsreq_nodetofh {
	struct puffs_req	pvfsr_pr;

	void			*pvfsr_fhcookie;	/* OUT	*/

	size_t			pvfsr_dsize;		/* OUT/IN  */
	uint8_t			pvfsr_data[0]		/* IN, XXX */
				    __aligned(ALIGNBYTES+1);
};

struct puffs_vfsreq_suspend {
	struct puffs_req	pvfsr_pr;

	int			pvfsr_status;
};
#define PUFFS_SUSPEND_START	0
#define PUFFS_SUSPEND_SUSPENDED	1
#define PUFFS_SUSPEND_RESUME	2
#define PUFFS_SUSPEND_ERROR	3

/*
 * aux structures for vnode operations.
 */

struct puffs_vnreq_lookup {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cn_cid;		/* OUT	*/

	void			*pvnr_newnode;		/* IN	*/
	enum vtype		pvnr_vtype;		/* IN	*/
	voff_t			pvnr_size;		/* IN	*/
	dev_t			pvnr_rdev;		/* IN	*/
};

struct puffs_vnreq_create {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cn_cid;		/* OUT	*/

	struct vattr		pvnr_va;		/* OUT	*/
	void			*pvnr_newnode;		/* IN	*/
};

struct puffs_vnreq_mknod {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cn_cid;		/* OUT	*/

	struct vattr		pvnr_va;		/* OUT	*/
	void			*pvnr_newnode;		/* IN	*/
};

struct puffs_vnreq_open {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cid;		/* OUT	*/
	int			pvnr_mode;		/* OUT	*/
};

struct puffs_vnreq_close {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cid;		/* OUT	*/
	int			pvnr_fflag;		/* OUT	*/
};

struct puffs_vnreq_access {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cid;		/* OUT	*/
	int			pvnr_mode;		/* OUT	*/
};

#define puffs_vnreq_setattr puffs_vnreq_setgetattr
#define puffs_vnreq_getattr puffs_vnreq_setgetattr
struct puffs_vnreq_setgetattr {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cid;		/* OUT	*/
	struct vattr		pvnr_va;		/* IN/OUT (op depend) */
};

#define puffs_vnreq_read puffs_vnreq_readwrite
#define puffs_vnreq_write puffs_vnreq_readwrite
struct puffs_vnreq_readwrite {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	  */
	off_t			pvnr_offset;		/* OUT	  */
	size_t			pvnr_resid;		/* IN/OUT */
	int			pvnr_ioflag;		/* OUT	  */

	uint8_t			pvnr_data[0];		/* IN/OUT (wr/rd) */
};

#define puffs_vnreq_ioctl puffs_vnreq_fcnioctl
#define puffs_vnreq_fcntl puffs_vnreq_fcnioctl
struct puffs_vnreq_fcnioctl {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;
	u_long			pvnr_command;
	pid_t			pvnr_pid;
	int			pvnr_fflag;

	void			*pvnr_data;
	size_t			pvnr_datalen;
	int			pvnr_copyback;
};

struct puffs_vnreq_poll {
	struct puffs_req	pvn_pr;

	int			pvnr_events;		/* IN/OUT */
	struct puffs_kcid	pvnr_cid;		/* OUT    */
};

struct puffs_vnreq_fsync {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cid;		/* OUT	*/
	off_t			pvnr_offlo;		/* OUT	*/
	off_t			pvnr_offhi;		/* OUT	*/
	int			pvnr_flags;		/* OUT	*/
};

struct puffs_vnreq_seek {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	*/
	off_t			pvnr_oldoff;		/* OUT	*/
	off_t			pvnr_newoff;		/* OUT	*/
};

struct puffs_vnreq_remove {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cn_cid;		/* OUT	*/

	void			*pvnr_cookie_targ;	/* OUT	*/
};

struct puffs_vnreq_mkdir {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cn_cid;		/* OUT	*/

	struct vattr		pvnr_va;		/* OUT	*/
	void			*pvnr_newnode;		/* IN	*/
};

struct puffs_vnreq_rmdir {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cn_cid;		/* OUT	*/

	void			*pvnr_cookie_targ;	/* OUT	*/
};

struct puffs_vnreq_link {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cn_cid;		/* OUT	*/

	void			*pvnr_cookie_targ;	/* OUT	*/
};

struct puffs_vnreq_rename {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn_src;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_src_cred;	/* OUT	*/
	struct puffs_kcid	pvnr_cn_src_cid;	/* OUT	*/
	struct puffs_kcn	pvnr_cn_targ;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_targ_cred;	/* OUT	*/
	struct puffs_kcid	pvnr_cn_targ_cid;	/* OUT	*/

	void			*pvnr_cookie_src;	/* OUT	*/
	void			*pvnr_cookie_targ;	/* OUT	*/
	void			*pvnr_cookie_targdir;	/* OUT	*/
};

struct puffs_vnreq_symlink {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cn_cid;		/* OUT	*/

	struct vattr		pvnr_va;		/* OUT	*/
	void			*pvnr_newnode;		/* IN	*/
	char			pvnr_link[MAXPATHLEN];	/* OUT	*/
};

struct puffs_vnreq_readdir {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	  */
	off_t			pvnr_offset;		/* IN/OUT */
	size_t			pvnr_resid;		/* IN/OUT */
	size_t			pvnr_ncookies;		/* IN/OUT */
	int			pvnr_eofflag;		/* IN     */

	size_t			pvnr_dentoff;		/* OUT    */
	uint8_t			pvnr_data[0]		/* IN  	  */
				    __aligned(ALIGNBYTES+1);
};

struct puffs_vnreq_readlink {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT */
	size_t			pvnr_linklen;		/* IN  */
	char			pvnr_link[MAXPATHLEN];	/* IN, XXX  */
};

struct puffs_vnreq_reclaim {
	struct puffs_req	pvn_pr;

	struct puffs_kcid	pvnr_cid;		/* OUT	*/
};

struct puffs_vnreq_inactive {
	struct puffs_req	pvn_pr;

	struct puffs_kcid	pvnr_cid;		/* OUT	*/
};

struct puffs_vnreq_print {
	struct puffs_req	pvn_pr;

	/* empty */
};

struct puffs_vnreq_pathconf {
	struct puffs_req	pvn_pr;

	int			pvnr_name;		/* OUT	*/
	int			pvnr_retval;		/* IN	*/
};

struct puffs_vnreq_advlock {
	struct puffs_req	pvn_pr;

	struct flock		pvnr_fl;		/* OUT	*/
	void			*pvnr_id;		/* OUT	*/
	int			pvnr_op;		/* OUT	*/
	int			pvnr_flags;		/* OUT	*/
};

struct puffs_vnreq_mmap {
	struct puffs_req	pvn_pr;

	vm_prot_t		pvnr_prot;		/* OUT	*/
	struct puffs_kcred	pvnr_cred;		/* OUT	*/
	struct puffs_kcid	pvnr_cid;		/* OUT	*/
};


/*
 * For cache reports.  Everything is always out-out-out, no replies
 */

struct puffs_cacherun {
	off_t			pcache_runstart;
	off_t			pcache_runend;
};

/* cache info.  old used for write now */
struct puffs_cacheinfo {
	struct puffs_req	pcache_pr;

	int			pcache_type;
	size_t			pcache_nruns;		
	struct puffs_cacherun	pcache_runs[0];
};
#define PCACHE_TYPE_READ	0
#define PCACHE_TYPE_WRITE	1

/* notyet */
#if 0
struct puffs_vnreq_kqfilter { };
struct puffs_vnreq_islocked { };
#endif
struct puffs_vnreq_getextattr { };
struct puffs_vnreq_setextattr { };
struct puffs_vnreq_listextattr { };

#ifdef _KERNEL
#define PUFFS_VFSREQ(a)							\
	struct puffs_vfsreq_##a a##_arg;				\
	memset(&a##_arg, 0, sizeof(struct puffs_vfsreq_##a))

#define PUFFS_VNREQ(a)							\
	struct puffs_vnreq_##a a##_arg;					\
	memset(&a##_arg, 0, sizeof(struct puffs_vnreq_##a))
#endif

#endif /* _PUFFS_MSGIF_H_ */

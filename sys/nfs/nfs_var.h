/*	$NetBSD: nfs_var.h,v 1.41.2.3 2004/08/18 10:19:08 skrll Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

/*
 * XXX needs <nfs/rpcv2.h> and <nfs/nfs.h> because of typedefs
 */

#ifdef _KERNEL
#include <sys/mallocvar.h>
#include <sys/pool.h>

MALLOC_DECLARE(M_NFSREQ);
MALLOC_DECLARE(M_NFSMNT);
MALLOC_DECLARE(M_NFSUID);
MALLOC_DECLARE(M_NFSD);
MALLOC_DECLARE(M_NFSDIROFF);
MALLOC_DECLARE(M_NFSBIGFH);
MALLOC_DECLARE(M_NQLEASE);
extern struct pool nfs_srvdesc_pool;

struct vnode;
struct uio;
struct ucred;
struct proc;
struct buf;
struct nfs_diskless;
struct sockaddr_in;
struct nfs_dlmount;
struct vnode;
struct nfsd;
struct mbuf;
struct file;
struct nqlease;
struct nqhost;
struct nfssvc_sock;
struct nfsmount;
struct socket;
struct nfsreq;
struct vattr;
struct nameidata;
struct nfsnode;
struct sillyrename;
struct componentname;
struct nfsd_srvargs;
struct nfsrv_descript;
struct nfs_fattr;
struct nfsdircache;
union nethostaddr;

/* nfs_bio.c */
int nfs_bioread __P((struct vnode *, struct uio *, int, struct ucred *, int));
struct buf *nfs_getcacheblk __P((struct vnode *, daddr_t, int, struct lwp *));
int nfs_vinvalbuf __P((struct vnode *, int, struct ucred *, struct lwp *,
		       int));
int nfs_asyncio __P((struct buf *));
int nfs_doio __P((struct buf *, struct lwp *));

/* nfs_boot.c */
/* see nfsdiskless.h */

/* nfs_kq.c */
void nfs_kqinit __P((void));

/* nfs_node.c */
void nfs_nhinit __P((void));
void nfs_nhreinit __P((void));
void nfs_nhdone __P((void));
int nfs_nget1 __P((struct mount *, nfsfh_t *, int, struct nfsnode **, int, struct lwp *));
#define	nfs_nget(mp, fhp, fhsize, npp, l) \
	nfs_nget1((mp), (fhp), (fhsize), (npp), 0, l)

/* nfs_vnops.c */
int nfs_null __P((struct vnode *, struct ucred *, struct lwp *));
int nfs_setattrrpc __P((struct vnode *, struct vattr *, struct ucred *,
			struct lwp *));
int nfs_readlinkrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_readrpc __P((struct vnode *, struct uio *));
int nfs_writerpc __P((struct vnode *, struct uio *, int *, boolean_t,
			boolean_t *));
int nfs_mknodrpc __P((struct vnode *, struct vnode **, struct componentname *,
		      struct vattr *));
int nfs_removeit __P((struct sillyrename *));
int nfs_removerpc __P((struct vnode *, const char *, int, struct ucred *,
		       struct lwp *));
int nfs_renameit __P((struct vnode *, struct componentname *,
		      struct sillyrename *));
int nfs_renamerpc __P((struct vnode *, const char *, int, struct vnode *,
		       const char *, int, struct ucred *, struct lwp *));
int nfs_readdirrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_readdirplusrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_sillyrename __P((struct vnode *, struct vnode *,
			 struct componentname *));
int nfs_lookitup __P((struct vnode *, const char *, int, struct ucred *,
		      struct lwp *, struct nfsnode **));
int nfs_commit __P((struct vnode *, off_t, uint32_t, struct lwp *));
int nfs_flush __P((struct vnode *, struct ucred *, int, struct lwp *, int));

/* nfs_nqlease.c */
void nqnfs_lease_updatetime __P((int));
void nqnfs_clientlease __P((struct nfsmount *, struct nfsnode *, int, int,
			    time_t, u_quad_t));
void nqsrv_locklease __P((struct nqlease *));
void nqsrv_unlocklease __P((struct nqlease *));
int nqsrv_getlease __P((struct vnode *, u_int32_t *, int, struct nfssvc_sock *,
			struct lwp *, struct mbuf *, int *, u_quad_t *,
			struct ucred *));
void nqsrv_addhost __P((struct nqhost *, struct nfssvc_sock *, struct mbuf *));
void nqsrv_instimeq __P((struct nqlease *, u_int32_t));
int nqsrv_cmpnam __P((struct nfssvc_sock *, struct mbuf *, struct nqhost *));
void nqsrv_send_eviction __P((struct vnode *, struct nqlease *,
			      struct nfssvc_sock *, struct mbuf *,
			      struct ucred *, struct lwp *));
void nqsrv_waitfor_expiry __P((struct nqlease *));
void nqnfs_serverd __P((void));
int nqnfsrv_getlease __P((struct nfsrv_descript *, struct nfssvc_sock *,
			  struct lwp *, struct mbuf **));
int nqnfsrv_vacated __P((struct nfsrv_descript *, struct nfssvc_sock *,
			 struct lwp *, struct mbuf **));
int nqnfs_getlease __P((struct vnode *, int, struct ucred *, struct lwp *));
int nqnfs_vacated __P((struct vnode *, struct ucred *, struct lwp *));
int nqnfs_callback __P((struct nfsmount *, struct mbuf *, struct mbuf *,
			caddr_t, struct lwp *));

/* nfs_serv.c */
int nfsrv3_access __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct lwp *, struct mbuf **));
int nfsrv_getattr __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct lwp *, struct mbuf **));
int nfsrv_setattr __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct lwp *, struct mbuf **));
int nfsrv_lookup __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct lwp *, struct mbuf **));
int nfsrv_readlink __P((struct nfsrv_descript *, struct nfssvc_sock *,
			struct lwp *, struct mbuf **));
int nfsrv_read __P((struct nfsrv_descript *, struct nfssvc_sock *,
		    struct lwp *, struct mbuf **));
int nfsrv_write __P((struct nfsrv_descript *, struct nfssvc_sock *,
		     struct lwp *, struct mbuf **));
int nfsrv_writegather __P((struct nfsrv_descript **, struct nfssvc_sock *,
			   struct lwp *, struct mbuf **));
void nfsrvw_coalesce __P((struct nfsrv_descript *, struct nfsrv_descript *));
int nfsrv_create __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct lwp *, struct mbuf **));
int nfsrv_mknod __P((struct nfsrv_descript *, struct nfssvc_sock *,
		     struct lwp *, struct mbuf **));
int nfsrv_remove __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct lwp *, struct mbuf **));
int nfsrv_rename __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct lwp *, struct mbuf **));
int nfsrv_link __P((struct nfsrv_descript *, struct nfssvc_sock *,
		    struct lwp *, struct mbuf **));
int nfsrv_symlink __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct lwp *, struct mbuf **));
int nfsrv_mkdir __P((struct nfsrv_descript *, struct nfssvc_sock *,
		     struct lwp *, struct mbuf **));
int nfsrv_rmdir __P((struct nfsrv_descript *, struct nfssvc_sock *,
		     struct lwp *, struct mbuf **));
int nfsrv_readdir __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct lwp *, struct mbuf **));
int nfsrv_readdirplus __P((struct nfsrv_descript *, struct nfssvc_sock *,
			   struct lwp *, struct mbuf **));
int nfsrv_commit __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct lwp *, struct mbuf **));
int nfsrv_statfs __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct lwp *, struct mbuf **));
int nfsrv_fsinfo __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct lwp *, struct mbuf **));
int nfsrv_pathconf __P((struct nfsrv_descript *, struct nfssvc_sock *,
		        struct lwp *, struct mbuf **));
int nfsrv_null __P((struct nfsrv_descript *, struct nfssvc_sock *,
		    struct lwp *, struct mbuf **));
int nfsrv_noop __P((struct nfsrv_descript *, struct nfssvc_sock *,
		    struct lwp *, struct mbuf **));
int nfsrv_access __P((struct vnode *, int, struct ucred *, int, struct lwp *,
		      int));

/* nfs_socket.c */
int nfs_connect __P((struct nfsmount *, struct nfsreq *, struct lwp *));
int nfs_reconnect __P((struct nfsreq *, struct lwp *));
void nfs_disconnect __P((struct nfsmount *));
void nfs_safedisconnect __P((struct nfsmount *));
int nfs_send __P((struct socket *, struct mbuf *, struct mbuf *,
		  struct nfsreq *, struct lwp *));
int nfs_receive __P((struct nfsreq *, struct mbuf **, struct mbuf **,
		     struct lwp *));
int nfs_reply __P((struct nfsreq *, struct lwp *));
int nfs_request __P((struct nfsnode *, struct mbuf *, int, struct lwp *,
		     struct ucred *, struct mbuf **, struct mbuf **,
		     caddr_t *, int *));
int nfs_rephead __P((int, struct nfsrv_descript *, struct nfssvc_sock *,
		     int, int, u_quad_t *, struct mbuf **, struct mbuf **,			     caddr_t *));
void nfs_timer __P((void *));
int nfs_sigintr __P((struct nfsmount *, struct nfsreq *, struct lwp *));
int nfs_sndlock __P((int *, struct nfsreq *));
void nfs_exit __P((struct proc *, void *));
void nfs_sndunlock __P((int *));
int nfs_rcvlock __P((struct nfsreq *));
void nfs_rcvunlock __P((struct nfsmount *));
int nfs_getreq __P((struct nfsrv_descript *, struct nfsd *, int));
int nfs_msg __P((struct lwp *, char *, char *));
void nfsrv_rcv __P((struct socket *, caddr_t, int));
int nfsrv_getstream __P((struct nfssvc_sock *, int));
int nfsrv_dorec __P((struct nfssvc_sock *, struct nfsd *,
		     struct nfsrv_descript **));
void nfsrv_wakenfsd __P((struct nfssvc_sock *));

/* nfs_srvcache.c */
void nfsrv_initcache __P((void ));
int nfsrv_getcache __P((struct nfsrv_descript *, struct nfssvc_sock *,
			struct mbuf **));
void nfsrv_updatecache __P((struct nfsrv_descript *, int, struct mbuf *));
void nfsrv_cleancache __P((void));

/* nfs_subs.c */
struct mbuf *nfsm_reqh __P((struct nfsnode *, u_long, int, caddr_t *));
struct mbuf *nfsm_rpchead __P((struct ucred *, int, int, int, int, char *, int,
			       char *, struct mbuf *, int, struct mbuf **,
			       u_int32_t *));
int nfsm_mbuftouio __P((struct mbuf **, struct uio *, int, caddr_t *));
int nfsm_uiotombuf __P((struct uio *, struct mbuf **, int, caddr_t *));
int nfsm_disct __P((struct mbuf **, caddr_t *, int, int, caddr_t *));
int nfs_adv __P((struct mbuf **, caddr_t *, int, int));
int nfsm_strtmbuf __P((struct mbuf **, char **, const char *, long));
u_long nfs_dirhash __P((off_t));
void nfs_initdircache __P((struct vnode *));
void nfs_initdirxlatecookie __P((struct vnode *));
struct nfsdircache *nfs_searchdircache __P((struct vnode *, off_t, int, int *));
struct nfsdircache *nfs_enterdircache __P((struct vnode *, off_t, off_t,						   int, daddr_t));
void nfs_invaldircache __P((struct vnode *, int));
void nfs_init __P((void));
int nfsm_loadattrcache __P((struct vnode **, struct mbuf **, caddr_t *,
			   struct vattr *, int flags));
int nfs_loadattrcache __P((struct vnode **, struct nfs_fattr *,
			   struct vattr *, int flags));
int nfs_getattrcache __P((struct vnode *, struct vattr *));
void nfs_delayedtruncate __P((struct vnode *));
int nfs_namei __P((struct nameidata *, fhandle_t *, uint32_t,
		   struct nfssvc_sock *, struct mbuf *, struct mbuf **,
		   caddr_t *, struct vnode **, struct lwp *, int, int));
void nfs_zeropad __P((struct mbuf *, int, int));
void nfsm_srvwcc __P((struct nfsrv_descript *, int, struct vattr *, int,
		      struct vattr *, struct mbuf **, char **));
void nfsm_srvpostopattr __P((struct nfsrv_descript *, int, struct vattr *,
			     struct mbuf **, char **));
void nfsm_srvfattr __P((struct nfsrv_descript *, struct vattr *,
			struct nfs_fattr *));
int nfsrv_fhtovp __P((fhandle_t *, int, struct vnode **, struct ucred *,
		      struct nfssvc_sock *, struct mbuf *, int *, int,
		      int, struct lwp *));
int nfsrv_setpublicfs __P((struct mount *, struct netexport *,
			   struct export_args *));
int nfs_ispublicfh __P((fhandle_t *));
int netaddr_match __P((int, union nethostaddr *, struct mbuf *));

/* flags for nfs_loadattrcache and friends */
#define	NAC_NOTRUNC	1	/* don't truncate file size */

void nfs_clearcommit __P((struct mount *));
void nfs_merge_commit_ranges __P((struct vnode *));
int nfs_in_committed_range __P((struct vnode *, off_t, off_t));
int nfs_in_tobecommitted_range __P((struct vnode *, off_t, off_t));
void nfs_add_committed_range __P((struct vnode *, off_t, off_t));
void nfs_del_committed_range __P((struct vnode *, off_t, off_t));
void nfs_add_tobecommitted_range __P((struct vnode *, off_t, off_t));
void nfs_del_tobecommitted_range __P((struct vnode *, off_t, off_t));

int nfsrv_errmap __P((struct nfsrv_descript *, int));
void nfsrvw_sort __P((gid_t *, int));
void nfsrv_setcred __P((struct ucred *, struct ucred *));
void nfs_cookieheuristic __P((struct vnode *, int *, struct lwp *,
			      struct ucred *));

u_int32_t nfs_getxid __P((void));
void nfs_renewxid __P((struct nfsreq *));

/* nfs_syscalls.c */
int sys_getfh __P((struct lwp *, void *, register_t *));
int sys_nfssvc __P((struct lwp *, void *, register_t *));
int nfssvc_addsock __P((struct file *, struct mbuf *));
int nfssvc_nfsd __P((struct nfsd_srvargs *, caddr_t, struct lwp *));
void nfsrv_zapsock __P((struct nfssvc_sock *));
void nfsrv_slpderef __P((struct nfssvc_sock *));
void nfsrv_init __P((int));
int nfssvc_iod __P((struct lwp *));
void nfs_iodinit __P((void));
void start_nfsio __P((void *));
void nfs_getset_niothreads __P((int));
int nfs_getauth __P((struct nfsmount *, struct nfsreq *, struct ucred *,
		     char **, int *, char *, int *, NFSKERBKEY_T));
int nfs_getnickauth __P((struct nfsmount *, struct ucred *, char **, int *,
			 char *, int));
int nfs_savenickauth __P((struct nfsmount *, struct ucred *, int, NFSKERBKEY_T,
			  struct mbuf **, char **, struct mbuf *));
#endif /* _KERNEL */

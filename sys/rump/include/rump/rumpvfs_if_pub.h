/*	$NetBSD: rumpvfs_if_pub.h,v 1.3.2.1 2010/04/30 14:44:29 uebayasi Exp $	*/

/*
 * Automatically generated.  DO NOT EDIT.
 * from: NetBSD: rumpvfs.ifspec,v 1.1 2009/10/14 17:17:00 pooka Exp 
 * by:   NetBSD: makerumpif.sh,v 1.4 2009/10/15 00:29:19 pooka Exp 
 */

void rump_pub_getvninfo(struct vnode *, enum vtype *, off_t *, dev_t *);
struct vfsops * rump_pub_vfslist_iterate(struct vfsops *);
struct vfsops * rump_pub_vfs_getopsbyname(const char *);
struct vattr * rump_pub_vattr_init(void);
void rump_pub_vattr_settype(struct vattr *, enum vtype);
void rump_pub_vattr_setmode(struct vattr *, mode_t);
void rump_pub_vattr_setrdev(struct vattr *, dev_t);
void rump_pub_vattr_free(struct vattr *);
void rump_pub_vp_incref(struct vnode *);
int rump_pub_vp_getref(struct vnode *);
void rump_pub_vp_rele(struct vnode *);
void rump_pub_vp_interlock(struct vnode *);
int rump_pub_etfs_register(const char *, const char *, enum rump_etfs_type);
int rump_pub_etfs_register_withsize(const char *, const char *, enum rump_etfs_type, uint64_t, uint64_t);
int rump_pub_etfs_remove(const char *);
void rump_pub_freecn(struct componentname *, int);
int rump_pub_checksavecn(struct componentname *);
int rump_pub_namei(uint32_t, uint32_t, const char *, struct vnode **, struct vnode **, struct componentname **);
struct componentname * rump_pub_makecn(u_long, u_long, const char *, size_t, struct kauth_cred *, struct lwp *);
int rump_pub_vfs_unmount(struct mount *, int);
int rump_pub_vfs_root(struct mount *, struct vnode **, int);
int rump_pub_vfs_statvfs(struct mount *, struct statvfs *);
int rump_pub_vfs_sync(struct mount *, int, struct kauth_cred *);
int rump_pub_vfs_fhtovp(struct mount *, struct fid *, struct vnode **);
int rump_pub_vfs_vptofh(struct vnode *, struct fid *, size_t *);
void rump_pub_vfs_syncwait(struct mount *);
int rump_pub_vfs_getmp(const char *, struct mount **);
void rump_pub_rcvp_set(struct vnode *, struct vnode *);
struct vnode * rump_pub_cdir_get(void);
int rump_pub_syspuffs_glueinit(int, int *);
int rump_pub_sys___stat30(const char *, struct stat *);
int rump_pub_sys___lstat30(const char *, struct stat *);
void rump_pub_vattr50_to_vattr(const struct vattr *, struct vattr *);
void rump_pub_vattr_to_vattr50(const struct vattr *, struct vattr *);

/* $NetBSD: udf_vfsops.c,v 1.5.2.1 2006/06/19 04:07:15 chap Exp $ */

/*
 * Copyright (c) 2006 Reinoud Zandijk
 * All rights reserved.
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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


#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: udf_vfsops.c,v 1.5.2.1 2006/06/19 04:07:15 chap Exp $");
#endif /* not lint */


#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kauth.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>

#include "udf.h"
#include "udf_subr.h"
#include "udf_bswap.h"


/* verbose levels of the udf filingsystem */
int udf_verbose = UDF_DEBUGGING;

/* malloc regions */
MALLOC_DEFINE(M_UDFMNT,   "UDF mount",		"UDF mount structures");
MALLOC_DEFINE(M_UDFVOLD,  "UDF volspace",	"UDF volume space descriptors");
MALLOC_DEFINE(M_UDFTEMP,  "UDF temp",		"UDF scrap space");
struct pool udf_node_pool;

/* supported functions predefined */
int udf_mountroot(void);
int udf_mount(struct mount *, const char *, void *, struct nameidata *, struct lwp *);
int udf_start(struct mount *, int, struct lwp *);
int udf_unmount(struct mount *, int, struct lwp *);
int udf_root(struct mount *, struct vnode **);
int udf_quotactl(struct mount *, int, uid_t, void *, struct lwp *);
int udf_statvfs(struct mount *, struct statvfs *, struct lwp *);
int udf_sync(struct mount *, int, kauth_cred_t, struct lwp *);
int udf_vget(struct mount *, ino_t, struct vnode **);
int udf_fhtovp(struct mount *, struct fid *, struct vnode **);
int udf_checkexp(struct mount *, struct mbuf *, int *, kauth_cred_t *);
int udf_vptofh(struct vnode *, struct fid *);
int udf_snapshot(struct mount *, struct vnode *, struct timespec *);

void udf_init(void);
void udf_reinit(void);
void udf_done(void);


/* internal functions */
static int udf_mountfs(struct vnode *, struct mount *, struct lwp *, struct udf_args *);
#if 0
static int update_mp(struct mount *, struct udf_args *);
#endif


/* handies */
#define	VFSTOUDF(mp)	((struct udf_mount *)mp->mnt_data)


/* --------------------------------------------------------------------- */

/* predefine vnode-op list descriptor */
extern const struct vnodeopv_desc udf_vnodeop_opv_desc;

const struct vnodeopv_desc * const udf_vnodeopv_descs[] = {
	&udf_vnodeop_opv_desc,
	NULL,
};


/* vfsops descriptor linked in as anchor point for the filingsystem */
struct vfsops udf_vfsops = {
	MOUNT_UDF,			/* vfs_name */
	udf_mount,
	udf_start,
	udf_unmount,
	udf_root,
	udf_quotactl,
	udf_statvfs,
	udf_sync,
	udf_vget,
	udf_fhtovp,
	udf_vptofh,
	udf_init,
	udf_reinit,
	udf_done,
	udf_mountroot,
	udf_snapshot,
	vfs_stdextattrctl,
	udf_vnodeopv_descs,
	/* int vfs_refcount   */
	/* LIST_ENTRY(vfsops) */
};
VFS_ATTACH(udf_vfsops);


/* --------------------------------------------------------------------- */

/* file system starts here */
void
udf_init(void)
{
	size_t size;

#ifdef _LKM
	malloc_type_attach(M_UDFMNT);
	malloc_type_attach(M_UDFVOLD);
	malloc_type_attach(M_UDFTEMP);
#endif

	/* init hashtables and pools */
	size = sizeof(struct udf_node);
	pool_init(&udf_node_pool, size, 0, 0, 0, "udf_node_pool", NULL);
}

/* --------------------------------------------------------------------- */

void
udf_reinit(void)
{
	/* recreate hashtables */
	/* reinit pool? */
}

/* --------------------------------------------------------------------- */

void
udf_done(void)
{
	/* remove hashtables and pools */
	pool_destroy(&udf_node_pool);

#ifdef _LKM
	malloc_type_detach(M_UDFMNT);
	malloc_type_detach(M_UDFVOLD);
	malloc_type_detach(M_UDFTEMP);
#endif
}

/* --------------------------------------------------------------------- */

int
udf_mountroot(void)
{
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

#define MPFREE(a, lst) \
	if ((a)) free((a), lst);
static void
free_udf_mountinfo(struct mount *mp)
{
	struct udf_mount *ump;
	int i;

	ump = VFSTOUDF(mp);
	if (ump) {
		mp->mnt_data = NULL;
		for (i = 0; i < UDF_ANCHORS; i++)
			MPFREE(ump->anchors[i], M_UDFVOLD);
		MPFREE(ump->primary_vol,      M_UDFVOLD);
		MPFREE(ump->logical_vol,      M_UDFVOLD);
		MPFREE(ump->unallocated,      M_UDFVOLD);
		MPFREE(ump->implementation,   M_UDFVOLD);
		MPFREE(ump->logvol_integrity, M_UDFVOLD);
		for (i = 0; i < UDF_PARTITIONS; i++)
			MPFREE(ump->partitions[i], M_UDFVOLD);
		MPFREE(ump->fileset_desc,     M_UDFVOLD);
		MPFREE(ump->vat_table,        M_UDFVOLD);
		MPFREE(ump->sparing_table,    M_UDFVOLD);

		/*
		 * Note that the node related (e)fe descriptors pool is
		 * destroyed already if it was used.
		 */

		free(ump, M_UDFMNT);
	}
}
#undef MPFREE

/* --------------------------------------------------------------------- */

int
udf_mount(struct mount *mp, const char *path,
	  void *data, struct nameidata *ndp, struct lwp *l)
{
	struct udf_args args;
	struct udf_mount *ump;
	struct vnode *devvp;
	struct proc *p;
	int openflags, accessmode, error;

	DPRINTF(CALL, ("udf_mount called\n"));

	p = l->l_proc;
	if (mp->mnt_flag & MNT_GETARGS) {
		/* request for the mount arguments */
		ump = VFSTOUDF(mp);
		if (ump == NULL)
			return EINVAL;
		return copyout(&ump->mount_args, data, sizeof(args));
	}

	/* handle request for updating mount parameters */
	/* TODO can't update my mountpoint yet */
	if (mp->mnt_flag & MNT_UPDATE) {
		return EOPNOTSUPP;
	}

	/* OK, so we are asked to mount the device/file! */
	error = copyin(data, &args, sizeof(struct udf_args));
	if (error)
		return error;

	/* check/translate struct version */
	/* TODO sanity checking other mount arguments */
	if (args.version != 1) {
		printf("mount_udf: unrecognized argument structure version\n");
		return EINVAL;
	}

	/* lookup name to get its vnode */
	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, args.fspec, l);
	error = namei(ndp);
	if (error)
		return error;

	/* devvp is *locked* now */
	devvp = ndp->ni_vp;
#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_VOLUMES)
		vprint("UDF mount, trying to mount \n", devvp);
#endif

	/* check if its a block device specified */
	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return ENOTBLK;
	}
	if (bdevsw_lookup(devvp->v_rdev) == NULL) {
		vrele(devvp);
		return ENXIO; 
	}

	/* force read-only for now */
	mp->mnt_flag |= MNT_RDONLY;

	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (kauth_cred_geteuid(p->p_cred) != 0) {
		accessmode = VREAD;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		error = VOP_ACCESS(devvp, accessmode, p->p_cred, l);
		if (error) {
			vput(devvp);
			return (error);
		}
	}

	/*
	 * Disallow multiple mounts of the same device.  Disallow mounting of
	 * a device that is currently in use (except for root, which might
	 * share swap device for miniroot).
	 */
	error = vfs_mountedon(devvp);
	if (error) {
		vput(devvp);
		return error;
	}
	if ((vcount(devvp) > 1) && (devvp != rootvp)) {
		vput(devvp);
		return EBUSY;
	}

	/*
	 * Open device and try to mount it!
	 */
	if (mp->mnt_flag & MNT_RDONLY) {
		openflags = FREAD;
	} else {
		openflags = FREAD | FWRITE;
	}
	error = VOP_OPEN(devvp, openflags, FSCRED, l);
	if (error == 0) {
		/* opened ok, try mounting */
		error = udf_mountfs(devvp, mp, l, &args);
		if (error) {
			free_udf_mountinfo(mp);
			/* devvp is still locked */
			(void) VOP_CLOSE(devvp, openflags, NOCRED, l);
		}
	}
	if (error) {
		/* devvp is still locked */
		vput(devvp);
		return error;
	}

	/* register our mountpoint being on this device */
	devvp->v_specmountpoint = mp;

	/* successfully mounted */
	DPRINTF(VOLUMES, ("udf_mount() successfull\n"));

	/* unlock it but dont deref it */
	VOP_UNLOCK(devvp, 0);

	return set_statvfs_info(path, UIO_USERSPACE, args.fspec, UIO_USERSPACE, mp, l);
}

/* --------------------------------------------------------------------- */

#ifdef DEBUG
static void
udf_unmount_sanity_check(struct mount *mp)
{
	struct vnode *vp;

	printf("On unmount, i found the following nodes:\n");
	LIST_FOREACH(vp, &mp->mnt_vnodelist, v_mntvnodes) {
		vprint("", vp);
		if (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) {
			printf("  is locked\n");
		}
		if (vp->v_usecount > 1)
			printf("  more than one usecount %d\n", vp->v_usecount);
	}
}
#endif


int
udf_unmount(struct mount *mp, int mntflags, struct lwp *l)
{
	struct udf_mount *ump;
	int error, flags, closeflags;

	DPRINTF(CALL, ("udf_umount called\n"));

	/*
	 * By specifying SKIPSYSTEM we can skip vnodes marked with VSYSTEM.
	 * This hardly documented feature allows us to exempt certain files
	 * from being flushed.
	 */
	flags = SKIPSYSTEM;	/* allow for system vnodes to stay alive */
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	ump = VFSTOUDF(mp);
	if (!ump)
		panic("UDF unmount: empty ump\n");

	/* TODO remove these paranoid functions */
#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_LOCKING)
		udf_unmount_sanity_check(mp);
#endif

	if ((error = vflush(mp, NULLVP, flags)) != 0)
		return error;

#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_LOCKING)
		udf_unmount_sanity_check(mp);
#endif

	DPRINTF(VOLUMES, ("flush OK\n"));

	/*
	 * TODO close logical volume and close session if requested.
	 *
	 * XXX no system nodes defined yet.  Code to reclaim them is calling
	 * VOP_RECLAIM on the nodes themselves.
	 */

	/* dispose of our descriptor pool */
	pool_destroy(&ump->desc_pool);

	/* close device */
	DPRINTF(VOLUMES, ("closing device\n"));
	if (mp->mnt_flag & MNT_RDONLY) {
		closeflags = FREAD;
	} else {
		closeflags = FREAD | FWRITE;
	}

	/* devvp is still locked by us */
	vn_lock(ump->devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(ump->devvp, closeflags, NOCRED, l);
	if (error)
		printf("Error during closure of device! error %d, "
		       "device might stay locked\n", error);
	DPRINTF(VOLUMES, ("device close ok\n"));

	/* clear our mount reference and release device node */
	ump->devvp->v_specmountpoint = NULL;
	vput(ump->devvp);

	/* free ump struct */
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;

	/* free up umt structure */
	free_udf_mountinfo(mp);

	DPRINTF(VOLUMES, ("Fin unmount\n"));
	return error;
}

/* --------------------------------------------------------------------- */

/*
 * Helper function of udf_mount() that actually mounts the disc.
 */

static int
udf_mountfs(struct vnode *devvp, struct mount *mp,
	    struct lwp *l, struct udf_args *args)
{
	struct udf_mount     *ump;
	uint32_t sector_size, lb_size, bshift;
	int    num_anchors, error, lst;

	/* flush out any old buffers remaining from a previous use. */
	error = vinvalbuf(devvp, V_SAVE, l->l_proc->p_cred, l, 0, 0);
	if (error)
		return error;

	/* allocate udf part of mount structure; malloc allways succeeds */
	ump = malloc(sizeof(struct udf_mount), M_UDFMNT, M_WAITOK);
	memset(ump, 0, sizeof(struct udf_mount));

	/* init locks */
	simple_lock_init(&ump->ihash_slock);
	lockinit(&ump->get_node_lock, PINOD, "udf_getnode", 0, 0);

	/* init `ino_t' to udf_node hash table */
	for (lst = 0; lst < UDF_INODE_HASHSIZE; lst++) {
		LIST_INIT(&ump->udf_nodes[lst]);
	}

	/* set up linkage */
	mp->mnt_data    = ump;
	ump->vfs_mountp = mp;

	/* set up arguments and device */
	ump->mount_args = *args;
	ump->devvp      = devvp;
	error = udf_update_discinfo(ump);

	if (error) { 
		printf("UDF mount: error inspecting fs node\n");
		return error;
	}

	/* inspect sector size */
	sector_size = ump->discinfo.sector_size;
	bshift = 1;
	while ((1 << bshift) < sector_size)
		bshift++;
	if ((1 << bshift) != sector_size) {
		printf("UDF mount: "
		       "hit NetBSD implementation fence on sector size\n");
		return EIO;
	}

	/* read all anchors to get volume descriptor sequence */
	num_anchors = udf_read_anchors(ump, args);
	if (num_anchors == 0)
		return ENOENT;

	DPRINTF(VOLUMES, ("Read %d anchors on this disc, session %d\n",
	    num_anchors, args->sessionnr));

	/* read in volume descriptor sequence */
	error = udf_read_vds_space(ump);
	if (error)
		printf("UDF mount: error reading volume space\n");

	/* check consistency and completeness */
	if (!error) {
		error = udf_process_vds(ump, args);
		if (error)
			printf("UDF mount: disc not properly formatted\n");
	}

	/*
	 * Initialise pool for descriptors associated with nodes. This is done
	 * in lb_size units though currently lb_size is dictated to be
	 * sector_size.
	 */
	lb_size = udf_rw32(ump->logical_vol->lb_size);
	pool_init(&ump->desc_pool, lb_size, 0, 0, 0, "udf_desc_pool", NULL);

	/* read vds support tables like VAT, sparable etc. */
	if (!error) {
		error = udf_read_vds_tables(ump, args);
		if (error)
			printf("UDF mount: error in format or damaged disc\n");
	}
	if (!error) {
		error = udf_read_rootdirs(ump, args);
		if (error)
			printf("UDF mount: "
			       "disc not properly formatted or damaged disc\n");
	}
	if (error)
		return error;

	/* setup rest of mount information */
	mp->mnt_data = ump;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (uint32_t) devvp->v_rdev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_UDF);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = UDF_MAX_NAMELEN;
	mp->mnt_flag |= MNT_LOCAL;

	/* bshift is allways equal to disc sector size */
	mp->mnt_dev_bshift = bshift;
	mp->mnt_fs_bshift  = bshift;

	/* do we have to set this? */
	devvp->v_specmountpoint = mp;

	/* success! */
	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_start(struct mount *mp, int flags, struct lwp *l)
{
	/* do we have to do something here? */
	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;
	struct long_ad *dir_loc;
	struct udf_mount *ump = VFSTOUDF(mp);
	struct udf_node *root_dir;
	int error;

	DPRINTF(CALL, ("udf_root called\n"));

	dir_loc = &ump->fileset_desc->rootdir_icb;
	error = udf_get_node(ump, dir_loc, &root_dir);

	if (!root_dir)
		error = ENOENT;
	if (error)
		return error;

	vp = root_dir->vnode;
	simple_lock(&vp->v_interlock);
		root_dir->vnode->v_flag |= VROOT;
	simple_unlock(&vp->v_interlock);

	*vpp = vp;
	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_quotactl(struct mount *mp, int cmds, uid_t uid, void *arg, struct lwp *l)
{
	DPRINTF(NOTIMPL, ("udf_quotactl called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

int
udf_statvfs(struct mount *mp, struct statvfs *sbp, struct lwp *l)
{
	struct udf_mount *ump = VFSTOUDF(mp);
	struct logvol_int_desc *lvid;
	struct udf_logvol_info *impl;
	uint64_t freeblks, sizeblks;
	uint32_t *pos1, *pos2;
	int part, num_part;

	DPRINTF(CALL, ("udf_statvfs called\n"));
	sbp->f_flag   = mp->mnt_flag;
	sbp->f_bsize  = ump->discinfo.sector_size;
	sbp->f_frsize = ump->discinfo.sector_size;
	sbp->f_iosize = ump->discinfo.sector_size;

	/* TODO integrity locking */
	/* free and used space for mountpoint based on logvol integrity */
	lvid = ump->logvol_integrity;
	if (lvid) {
		num_part = udf_rw32(lvid->num_part);
		impl = (struct udf_logvol_info *) (lvid->tables + 2*num_part);

		freeblks = sizeblks = 0;
		for (part=0; part < num_part; part++) {
			pos1 = &lvid->tables[0] + part;
			pos2 = &lvid->tables[0] + num_part + part;
			if (udf_rw32(*pos1) != (uint32_t) -1) {
				freeblks += udf_rw32(*pos1);
				sizeblks += udf_rw32(*pos2);
			}
		}
		sbp->f_blocks = sizeblks;
		sbp->f_bfree  = freeblks;
		sbp->f_files  = udf_rw32(impl->num_files);
		sbp->f_files += udf_rw32(impl->num_directories);

		/* XXX read only for now XXX */
		sbp->f_bavail = 0;
		sbp->f_bresvd = 0;

		/* tricky, next only aplies to ffs i think, so set to zero */
		sbp->f_ffree  = 0;
		sbp->f_favail = 0;
		sbp->f_fresvd = 0;
	}

	copy_statvfs_info(sbp, mp);
	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_sync(struct mount *mp, int waitfor, kauth_cred_t cred, struct lwp *p)
{
	DPRINTF(CALL, ("udf_sync called\n"));
	/* nothing to be done as upto now read-only */
	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Get vnode for the file system type specific file id ino for the fs. Its
 * used for reference to files by unique ID and for NFSv3.
 * (optional) TODO lookup why some sources state NFSv3
 */
int
udf_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	DPRINTF(NOTIMPL, ("udf_vget called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/*
 * Lookup vnode for file handle specified
 */
int
udf_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	DPRINTF(NOTIMPL, ("udf_fhtovp called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/*
 * Create an unique file handle. Its structure is opaque and won't be used by
 * other subsystems. It should uniquely identify the file in the filingsystem
 * and enough information to know if a file has been removed and/or resources
 * have been recycled.
 */
int
udf_vptofh(struct vnode *vp, struct fid *fid)
{
	DPRINTF(NOTIMPL, ("udf_vptofh called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/*
 * Create a filingsystem snapshot at the specified timestamp. Could be
 * implemented by explicitly creating a new session or with spare room in the
 * integrity descriptor space
 */
int
udf_snapshot(struct mount *mp, struct vnode *vp, struct timespec *tm)
{
	DPRINTF(NOTIMPL, ("udf_snapshot called\n"));
	return EOPNOTSUPP;
}



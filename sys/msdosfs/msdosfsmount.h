/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 * 
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 * 
 * This software is provided "as is".
 * 
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 * 
 * October 1992
 * 
 *	$Id: msdosfsmount.h,v 1.1.2.1 1993/09/24 08:53:35 mycroft Exp $
 */

/*
 * Layout of the mount control block for a msdos file system.
 */
struct msdosfsmount {
	struct mount *pm_mountp;/* vfs mount struct for this fs */
	dev_t pm_dev;		/* block special device mounted */
	struct vnode *pm_devvp;	/* vnode for block device mntd */
	struct bpb50 pm_bpb;	/* BIOS parameter blk for this fs */
	u_long pm_fatblk;	/* block # of first FAT */
	u_long pm_rootdirblk;	/* block # of root directory */
	u_long pm_rootdirsize;	/* size in blocks (not clusters) */
	u_long pm_firstcluster;	/* block number of first cluster */
	u_long pm_nmbrofclusters;	/* # of clusters in filesystem */
	u_long pm_maxcluster;	/* maximum cluster number */
	u_long pm_freeclustercount;	/* number of free clusters */
	u_long pm_lookhere;	/* start free cluster search here */
	u_long pm_bnshift;	/* shift file offset right this amount to get a block number */
	u_long pm_brbomask;	/* and a file offset with this mask to get block rel offset */
	u_long pm_cnshift;	/* shift file offset right this amount to get a cluster number */
	u_long pm_crbomask;	/* and a file offset with this mask to get cluster rel offset */
	u_long pm_bpcluster;	/* bytes per cluster */
	u_long pm_depclust;	/* directory entries per cluster */
	u_long pm_fmod;		/* ~0 if fs is modified, this can rollover to 0	*/
	u_long pm_fatblocksize;	/* size of fat blocks in bytes */
	u_long pm_fatblocksec;	/* size of fat blocks in sectors */
	u_long pm_fatsize;	/* size of fat in bytes */
	u_char *pm_inusemap;	/* ptr to bitmap of in-use clusters */
	char pm_ronly;		/* read only if non-zero */
	char pm_waitonfat;	/* wait for writes of the fat to complt, when 0 use bdwrite, else use bwrite */
};

/*
 * How to compute pm_cnshift and pm_crbomask.
 * 
 * pm_crbomask = (pm_SectPerClust * pm_BytesPerSect) - 1 
 * if (bytesperclust == * 0) 
 * 	return EBADBLKSZ; 
 * bit = 1; 
 * for (i = 0; i < 32; i++) { 
 *	if (bit & bytesperclust) { 
 *		if (bit ^ bytesperclust) 
 *			return EBADBLKSZ; 
 *		pm_cnshift = * i; 
 *		break; 
 *	} 
 *	bit <<= 1; 
 * }
 */

/*
 * Shorthand for fields in the bpb contained in the msdosfsmount structure.
 */
#define	pm_BytesPerSec	pm_bpb.bpbBytesPerSec
#define	pm_SectPerClust	pm_bpb.bpbSecPerClust
#define	pm_ResSectors	pm_bpb.bpbResSectors
#define	pm_FATs		pm_bpb.bpbFATs
#define	pm_RootDirEnts	pm_bpb.bpbRootDirEnts
#define	pm_Sectors	pm_bpb.bpbSectors
#define	pm_Media	pm_bpb.bpbMedia
#define	pm_FATsecs	pm_bpb.bpbFATsecs
#define	pm_SecPerTrack	pm_bpb.bpbSecPerTrack
#define	pm_Heads	pm_bpb.bpbHeads
#define	pm_HiddenSects	pm_bpb.bpbHiddenSecs
#define	pm_HugeSectors	pm_bpb.bpbHugeSectors

/*
 * Map a cluster number into a filesystem relative block number.
 */
#define	cntobn(pmp, cn) \
	((((cn)-CLUST_FIRST) * (pmp)->pm_SectPerClust) + (pmp)->pm_firstcluster)

/*
 * Map a filesystem relative block number back into a cluster number.
 */
#define	bntocn(pmp, bn) \
	((((bn) - pmp->pm_firstcluster)/ (pmp)->pm_SectPerClust) + CLUST_FIRST)

/*
 * Calculate block number for directory entry in root dir, offset dirofs
 */
#define	roottobn(pmp, dirofs) \
	(((dirofs) / (pmp)->pm_depclust) * (pmp)->pm_SectPerClust \
	+ (pmp)->pm_rootdirblk)

/*
 * Calculate block number for directory entry at cluster dirclu, offset
 * dirofs
 */
#define	detobn(pmp, dirclu, dirofs) \
	((dirclu) == MSDOSFSROOT \
	 ? roottobn((pmp), (dirofs)) \
	 : cntobn((pmp), (dirclu)))

/*
 * Convert pointer to buffer -> pointer to direntry
 */
#define	bptoep(pmp, bp, dirofs) \
	((struct direntry *)((bp)->b_un.b_addr)	\
	 + (dirofs) % (pmp)->pm_depclust)


/*
 * Prototypes for MSDOSFS virtual filesystem operations
 */
int msdosfs_mount __P((struct mount * mp, char *path, caddr_t data, struct nameidata * ndp, struct proc * p));
int msdosfs_start __P((struct mount * mp, int flags, struct proc * p));
int msdosfs_unmount __P((struct mount * mp, int mntflags, struct proc * p));
int msdosfs_root __P((struct mount * mp, struct vnode ** vpp));
int msdosfs_quotactl __P((struct mount * mp, int cmds, int uid, caddr_t arg, struct proc * p));
int msdosfs_statfs __P((struct mount * mp, struct statfs * sbp, struct proc * p));
int msdosfs_sync __P((struct mount * mp, int waitfor));
int msdosfs_fhtovp __P((struct mount * mp, struct fid * fhp, struct vnode ** vpp));
int msdosfs_vptofh __P((struct vnode * vp, struct fid * fhp));
int msdosfs_init __P(());

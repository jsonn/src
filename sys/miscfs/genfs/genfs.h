/*	$NetBSD: genfs.h,v 1.11.2.1 2001/06/21 20:07:33 nathanw Exp $	*/

int	genfs_badop		__P((void *));
int	genfs_nullop		__P((void *));
int	genfs_enoioctl		__P((void *));
int	genfs_enoextops		__P((void *));
int	genfs_einval		__P((void *));
int	genfs_eopnotsupp	__P((void *));
int	genfs_eopnotsupp_rele	__P((void *));
int	genfs_ebadf		__P((void *));
int	genfs_nolock		__P((void *));
int	genfs_noislocked	__P((void *));
int	genfs_nounlock		__P((void *));

int	genfs_poll		__P((void *));
int	genfs_fcntl		__P((void *));
int	genfs_fsync		__P((void *));
int	genfs_seek		__P((void *));
int	genfs_abortop		__P((void *));
int	genfs_revoke		__P((void *));
int	genfs_lease_check	__P((void *));
int	genfs_lock		__P((void *));
int	genfs_islocked		__P((void *));
int	genfs_unlock		__P((void *));
int	genfs_mmap		__P((void *));
int	genfs_getpages		__P((void *));
int	genfs_putpages		__P((void *));
int	genfs_size		__P((void *));

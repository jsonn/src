/*	$NetBSD: sa_dref_aoi.h,v 1.1.1.1.4.2 2008/10/19 22:39:38 haad Exp $	*/

/* $srcdir/conf/sa_dref/sa_dref_aoi.h */
#define	NFS_SA_DREF(dst, src) { \
		(dst)->addr->buf = (char *) (src); \
		(dst)->addr->len = sizeof(struct sockaddr_in); \
		(dst)->addr->maxlen = sizeof(struct sockaddr_in); \
	}
#define NFS_ARGS_T_ADDR_IS_POINTER 1

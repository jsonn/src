/*	$NetBSD: hn_dref_isc3.h,v 1.1.1.1.4.2 2000/06/07 00:52:21 dogcow Exp $ */
/* $srcdir/conf/hn_dref/hn_dref_isc3.h */
#define NFS_HN_DREF(dst, src) { \
		strncpy((dst), (src), MAXHOSTNAMELEN); \
		(dst)[MAXHOSTNAMELEN] = '\0'; \
	}

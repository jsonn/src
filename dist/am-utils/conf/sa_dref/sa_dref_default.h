/*	$NetBSD: sa_dref_default.h,v 1.1.1.1.4.2 2000/06/07 00:52:22 dogcow Exp $ */
/* $srcdir/conf/sa_dref/sa_dref_default.h */
#define	NFS_SA_DREF(dst, src) (dst)->addr = (src)

/*	$NetBSD: nfsfh.h,v 1.4.12.1 2002/06/06 20:00:45 he Exp $	*/

/*
 * Header: nfsfh.h,v 1.6 96/12/16 03:40:31 leres Exp 
 *
 * nfsfh.h - NFS file handle definitions (for portable use)
 *
 * Jeffrey C. Mogul
 * Digital Equipment Corporation
 * Western Research Laboratory
 */

/*
 * Internal representation of dev_t, because different NFS servers
 * that we might be spying upon use different external representations.
 */
typedef struct {
	u_int32_t Minor;	/* upper case to avoid clashing with macro names */
	u_int32_t Major;
} my_devt;

#define	dev_eq(a,b)	((a.Minor == b.Minor) && (a.Major == b.Major))

/*
 * Many file servers now use a large file system ID.  This is
 * our internal representation of that.
 */
typedef	struct {
	my_devt	Fsid_dev;		/* XXX avoid name conflict with AIX */
	u_int32_t fsid_code;
} my_fsid;

#define	fsid_eq(a,b)	((a.fsid_code == b.fsid_code) &&\
			 dev_eq(a.Fsid_dev, b.Fsid_dev))

extern void Parse_fh(const unsigned char *, int, my_fsid *, ino_t *, const char **, const char **, int);

/*	$NetBSD: version.h,v 1.14.2.3 2004/09/17 04:36:41 jmc Exp $	*/

#if defined(__KRB5_VERSION) && !defined(__NO_KRB4_VERSION)
#define	heimdal_long_version	__heimdal_long_version
#define	heimdal_version		__heimdal_version
#define	__NO_KRB4_VERSION
#endif

#if defined(__KRB4_VERSION) && !defined(__NO_KRB5_VERSION)
#define	krb4_long_version	__krb4_long_version
#define	krb4_version		__krb4_version
#define	__NO_KRB5_VERSION
#endif

#ifndef __NO_KRB5_VERSION
const char *heimdal_long_version = "@(#)$Version: Heimdal 0.6.3 (NetBSD) $";
const char *heimdal_version = "Heimdal 0.6.3";
#endif

#ifndef __NO_KRB4_VERSION
const char *krb4_long_version = "@(#)$Version: KTH-KRB 1.2 (NetBSD) $";
const char *krb4_version = "KTH-KRB 1.2";
#endif

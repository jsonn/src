/*	$NetBSD: netgroup.h,v 1.1.1.1.4.1 2007/02/10 19:20:48 tron Exp $	*/

#ifndef netgroup_h
#define netgroup_h
#ifndef __GLIBC__

/*
 * The standard is crazy.  These values "belong" to getnetgrent() and
 * shouldn't be altered by the caller.
 */
int getnetgrent __P((/* const */ char **, /* const */ char **,
		     /* const */ char **));

int getnetgrent_r __P((char **, char **, char **, char *, int));

void endnetgrent __P((void));

#ifdef __osf__
int innetgr __P((char *, char *, char *, char *));
void setnetgrent __P((char *));
#else
void setnetgrent __P((const char *));
int innetgr __P((const char *, const char *, const char *, const char *));
#endif
#endif
#endif

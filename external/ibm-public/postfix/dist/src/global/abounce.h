/*	$NetBSD: abounce.h,v 1.1.1.1.2.4 2011/08/09 18:58:16 riz Exp $	*/

#ifndef _ABOUNCE_H_INCLUDED_
#define _ABOUNCE_H_INCLUDED_

/*++
/* NAME
/*	abounce 3h
/* SUMMARY
/*	asynchronous bounce/defer service client
/* SYNOPSIS
/*	#include <abounce.h>
/* DESCRIPTION
/* .nf

 /*
  * Global library.
  */
#include <bounce.h>

 /*
  * Client interface.
  */
typedef void (*ABOUNCE_FN) (int, char *);

extern void abounce_flush(int, const char *, const char *, const char *, const char *, const char *, int, ABOUNCE_FN, char *);
extern void adefer_flush(int, const char *, const char *, const char *, const char *, const char *, int, ABOUNCE_FN, char *);
extern void adefer_warn(int, const char *, const char *, const char *, const char *, const char *, int, ABOUNCE_FN, char *);
extern void atrace_flush(int, const char *, const char *, const char *, const char *, const char *, int, ABOUNCE_FN, char *);

extern void abounce_flush_verp(int, const char *, const char *, const char *, const char *, const char *, int, const char *, ABOUNCE_FN, char *);
extern void adefer_flush_verp(int, const char *, const char *, const char *, const char *, const char *, int, const char *, ABOUNCE_FN, char *);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

#endif

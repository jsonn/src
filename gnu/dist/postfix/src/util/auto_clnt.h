/*	$NetBSD: auto_clnt.h,v 1.1.1.2.2.1 2006/11/20 13:30:59 tron Exp $	*/

#ifndef _AUTO_CLNT_H_INCLUDED_
#define _AUTO_CLNT_H_INCLUDED_

/*++
/* NAME
/*	auto_clnt 3h
/* SUMMARY
/*	client endpoint maintenance
/* SYNOPSIS
/*	#include <auto_clnt.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstream.h>

 /*
  * External interface.
  */
typedef struct AUTO_CLNT AUTO_CLNT;

extern AUTO_CLNT *auto_clnt_create(const char *, int, int, int);
extern VSTREAM *auto_clnt_access(AUTO_CLNT *);
extern void auto_clnt_recover(AUTO_CLNT *);
extern const char *auto_clnt_name(AUTO_CLNT *);
extern void auto_clnt_free(AUTO_CLNT *);

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

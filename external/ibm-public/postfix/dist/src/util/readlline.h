/*	$NetBSD: readlline.h,v 1.1.1.1.2.3 2011/01/07 01:24:19 riz Exp $	*/

#ifndef _READLINE_H_INCLUDED_
#define _READLINE_H_INCLUDED_

/*++
/* NAME
/*	readlline 3h
/* SUMMARY
/*	read logical line
/* SYNOPSIS
/*	#include <readlline.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstream.h>
#include <vstring.h>

 /*
  * External interface.
  */
extern VSTRING *readlline(VSTRING *, VSTREAM *, int *);

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

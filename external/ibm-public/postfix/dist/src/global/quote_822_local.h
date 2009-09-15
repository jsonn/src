/*	$NetBSD: quote_822_local.h,v 1.1.1.1.2.2 2009/09/15 06:02:52 snj Exp $	*/

#ifndef _QUOTE_822_H_INCLUDED_
#define _QUOTE_822_H_INCLUDED_

/*++
/* NAME
/*	quote_822_local 3h
/* SUMMARY
/*	quote local part of mailbox
/* SYNOPSIS
/*	#include "quote_822_local.h"
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Global library.
  */
#include <quote_flags.h>

 /*
  * External interface.
  */
extern VSTRING *quote_822_local_flags(VSTRING *, const char *, int);
extern VSTRING *unquote_822_local(VSTRING *, const char *);
#define quote_822_local(dst, src) \
	quote_822_local_flags((dst), (src), QUOTE_FLAG_8BITCLEAN)

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

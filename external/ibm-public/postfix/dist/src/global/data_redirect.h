/*	$NetBSD: data_redirect.h,v 1.1.1.1.2.2 2009/09/15 06:02:38 snj Exp $	*/

#ifndef _DATA_REDIRECT_H_INCLUDED_
#define _DATA_REDIRECT_H_INCLUDED_

/*++
/* NAME
/*	data_redirect 3h
/* SUMMARY
/*	redirect writes from legacy pathname to Postfix-owned data directory
/* SYNOPSIS
/*	#include "data_redirect.h"
/* DESCRIPTION
/* .nf

 /*
  * External interface.
  */
char   *data_redirect_file(VSTRING *, const char *);
char   *data_redirect_map(VSTRING *, const char *);

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

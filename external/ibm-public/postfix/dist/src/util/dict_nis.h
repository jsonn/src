/*	$NetBSD: dict_nis.h,v 1.1.1.1.4.2 2010/04/21 05:24:16 matt Exp $	*/

#ifndef _DIST_NIS_H_INCLUDED_
#define _DIST_NIS_H_INCLUDED_

/*++
/* NAME
/*	dict_nis 3h
/* SUMMARY
/*	dictionary manager interface to NIS maps
/* SYNOPSIS
/*	#include <dict_nis.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <dict.h>

 /*
  * External interface.
  */
#define DICT_TYPE_NIS	"nis"

extern DICT *dict_nis_open(const char *, int, int);

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

/*	$NetBSD: dict_dbm.h,v 1.1.1.1.2.2 2009/09/15 06:03:55 snj Exp $	*/

#ifndef _DICT_DBN_H_INCLUDED_
#define _DICT_DBN_H_INCLUDED_

/*++
/* NAME
/*	dict_dbm 3h
/* SUMMARY
/*	dictionary manager interface to DBM files
/* SYNOPSIS
/*	#include <dict_dbm.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <dict.h>

 /*
  * External interface.
  */
#define DICT_TYPE_DBM	"dbm"

extern DICT *dict_dbm_open(const char *, int, int);

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

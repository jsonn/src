/*	$NetBSD: dsb_scan.h,v 1.1.1.1.2.3 2011/01/07 01:24:02 riz Exp $	*/

#ifndef _DSB_SCAN_H_INCLUDED_
#define _DSB_SCAN_H_INCLUDED_

/*++
/* NAME
/*	dsb_scan 3h
/* SUMMARY
/*	write DSN to stream
/* SYNOPSIS
/*	#include <dsb_scan.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstream.h>
#include <attr.h>

 /*
  * Global library.
  */
#include <dsn_buf.h>

 /*
  * External interface.
  */
extern int dsb_scan(ATTR_SCAN_MASTER_FN, VSTREAM *, int, void *);

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

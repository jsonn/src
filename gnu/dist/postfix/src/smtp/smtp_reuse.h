/*	$NetBSD: smtp_reuse.h,v 1.1.1.1.2.2 2006/07/12 15:06:42 tron Exp $	*/

/*++
/* NAME
/*	smtp_reuse 3h
/* SUMMARY
/*	SMTP session cache glue
/* SYNOPSIS
/*	#include <smtp_reuse.h>
/* DESCRIPTION
/* .nf

 /*
  * DNS library.
  */
#include <dns.h>

 /*
  * Internal interfaces.
  */
extern void smtp_save_session(SMTP_STATE *);
extern SMTP_SESSION *smtp_reuse_domain(SMTP_STATE *, int, const char *, unsigned);
extern SMTP_SESSION *smtp_reuse_addr(SMTP_STATE *, DNS_RR *, unsigned);

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

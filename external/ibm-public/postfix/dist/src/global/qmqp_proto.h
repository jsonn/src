/*	$NetBSD: qmqp_proto.h,v 1.1.1.1.4.2 2010/04/21 05:23:59 matt Exp $	*/

/*++
/* NAME
/*	qmqpd 3h
/* SUMMARY
/*	QMQP protocol
/* SYNOPSIS
/*	include <qmqpd_proto.h>
/* DESCRIPTION
/* .nf

 /*
  * QMQP protocol status codes.
  */
#define QMQP_STAT_OK	'K'		/* success */
#define QMQP_STAT_RETRY	'Z'		/* recoverable error */
#define QMQP_STAT_HARD	'D'		/* unrecoverable error */

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

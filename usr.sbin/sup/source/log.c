/*	$NetBSD: log.c,v 1.3.4.1 1999/04/12 21:27:09 pk Exp $	*/

/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * Logging support for SUP
 **********************************************************************
 * HISTORY
 * Revision 1.5  92/08/11  12:03:43  mrt
 * 	Brad's delinting and variable argument list usage
 * 	changes. Added copyright.
 * 
 * Revision 1.3  89/08/15  15:30:37  bww
 * 	Updated to use v*printf() in place of _doprnt().
 * 	From "[89/04/19            mja]" at CMU.
 * 	[89/08/15            bww]
 * 
 * 27-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added check to allow logopen() to be called multiple times.
 *
 * 20-May-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Created.
 *
 **********************************************************************
 */

#include <stdio.h>
#include <sys/syslog.h>
#include <c.h>
#include "supcdefs.h"
#include "supextern.h"

static int opened = 0;

void
logopen(program)
char *program;
{
	if (opened)  return;
	openlog(program,LOG_PID,LOG_DAEMON);
	opened++;
}

void
#ifdef __STDC__
logquit(int retval,char *fmt,...)
#else
/*VARARGS*//*ARGSUSED*/
logquit(va_alist)
va_dcl
#endif
{
	char buf[STRINGLENGTH];
	va_list ap;

#ifdef __STDC__
	va_start(ap,fmt);
#else
	int retval;
	char *fmt;

	va_start(ap);
	retval = va_arg(ap,int);
	fmt = va_arg(ap,char *);
#endif
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (opened) {
		syslog (LOG_ERR,buf);
		closelog ();
		exit (retval);
	}
	quit (retval,"SUP: %s\n", buf);
}

void
#ifdef __STDC__
logerr(char *fmt,...)
#else
/*VARARGS*//*ARGSUSED*/
logerr(va_alist)
va_dcl
#endif
{
	char buf[STRINGLENGTH];
	va_list ap;

#ifdef __STDC__
	va_start(ap,fmt);
#else
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap,char *);
#endif
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (opened) {
		syslog (LOG_ERR,buf);
		return;
	}
	fprintf (stderr,"SUP: %s\n",buf);
	(void) fflush (stderr);
}

void
#ifdef __STDC__
loginfo(char *fmt,...)
#else
/*VARARGS*//*ARGSUSED*/
loginfo(va_alist)
va_dcl
#endif
{
	char buf[STRINGLENGTH];
	va_list ap;

#ifdef __STDC__
	va_start(ap,fmt);
#else
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap,char *);
#endif
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (opened) {
		syslog (LOG_INFO,buf);
		return;
	}
	printf ("%s\n",buf);
	(void) fflush (stdout);
}

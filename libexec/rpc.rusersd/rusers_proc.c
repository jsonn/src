/*	$NetBSD: rusers_proc.c,v 1.21.6.1 2000/06/22 15:58:35 minoura Exp $	*/

/*-
 *  Copyright (c) 1993 John Brezak
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: rusers_proc.c,v 1.21.6.1 2000/06/22 15:58:35 minoura Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <utmp.h>

#include <rpc/rpc.h>

#include "rusers_proc.h"

#ifdef XIDLE
#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/extensions/xidle.h>
#endif
#include <rpcsvc/rusers.h>	/* New version */
#include <rpcsvc/rnusers.h>	/* Old version */

#define	IGNOREUSER	"sleeper"

#ifdef OSF
#define _PATH_UTMP UTMP_FILE
#endif

#ifndef _PATH_UTMP
#define _PATH_UTMP "/etc/utmp"
#endif

#ifndef _PATH_DEV
#define _PATH_DEV "/dev"
#endif

#ifndef UT_LINESIZE
#define UT_LINESIZE sizeof(((struct utmp *)0)->ut_line)
#endif
#ifndef UT_NAMESIZE
#define UT_NAMESIZE sizeof(((struct utmp *)0)->ut_name)
#endif
#ifndef UT_HOSTSIZE
#define UT_HOSTSIZE sizeof(((struct utmp *)0)->ut_host)
#endif

typedef char ut_line_t[UT_LINESIZE];
typedef char ut_name_t[UT_NAMESIZE];
typedef char ut_host_t[UT_HOSTSIZE];

static struct rusers_utmp utmps[MAXUSERS];
static struct utmpidle *utmp_idlep[MAXUSERS];
static struct utmpidle utmp_idle[MAXUSERS];
static ut_line_t line[MAXUSERS];
static ut_name_t name[MAXUSERS];
static ut_host_t host[MAXUSERS];

extern int from_inetd;

static int getidle(char *, char *);
static int *rusers_num_svc(void *, struct svc_req *);
static utmp_array *do_names_3(int);
static struct utmpidlearr *do_names_2(int);

/* XXX */
struct utmpidlearr *rusersproc_names_2_svc(void *, struct svc_req *);
struct utmpidlearr *rusersproc_allnames_2_svc(void *, struct svc_req *);


#ifdef XIDLE
static Display *dpy;
static sigjmp_buf openAbort;

static int XqueryIdle __P((char *));
static void abortOpen __P((int));

static void
abortOpen(int n)
{
	siglongjmp(openAbort, 1);
}

static int
XqueryIdle(char *display)
{
	int first_event, first_error;
	Time IdleTime;

	(void) signal(SIGALRM, abortOpen);
	(void) alarm(10);
	if (!sigsetjmp(openAbort, 0)) {
		if ((dpy = XOpenDisplay(display)) == NULL) {
			syslog(LOG_DEBUG, "cannot open display %s", display);
			return (-1);
		}
		if (XidleQueryExtension(dpy, &first_event, &first_error)) {
			if (!XGetIdleTime(dpy, &IdleTime)) {
				syslog(LOG_DEBUG, "%s: unable to get idle time",
				    display);
				return (-1);
			}
		} else {
			syslog(LOG_DEBUG, "%s: Xidle extension not loaded",
			    display);
			return (-1);
		}
		XCloseDisplay(dpy);
	} else {
		syslog(LOG_DEBUG, "%s: server grabbed for over 10 seconds",
		    display);
		return (-1);
	}
	(void) alarm(0);
	(void) signal(SIGALRM, SIG_DFL);

	IdleTime /= 1000;
	return ((IdleTime + 30) / 60);
}
#endif /* XIDLE */

static int
getidle(char *tty, char *display)
{
	struct stat st;
	char devname[PATH_MAX];
	time_t now;
	long idle;
	
	/*
	 * If this is an X terminal or console, then try the
	 * XIdle extension
	 */
#ifdef XIDLE
	if (display && *display && strchr(display, ':') != NULL &&
	    (idle = XqueryIdle(display)) >= 0)
		return (idle);
#endif
	idle = 0;
	if (*tty == 'X') {
		long kbd_idle, mouse_idle;
#if !defined(i386)
		kbd_idle = getidle("kbd", NULL);
#else
		/*
		 * XXX Icky i386 console hack.
		 */
		kbd_idle = getidle("vga", NULL);
#endif
		mouse_idle = getidle("mouse", NULL);
		idle = (kbd_idle < mouse_idle) ? kbd_idle : mouse_idle;
	} else {
		snprintf(devname, sizeof devname, "%s/%s", _PATH_DEV, tty);
		if (stat(devname, &st) == -1) {
			syslog(LOG_WARNING, "Cannot stat %s (%m)", devname);
			return 0;
		}
		(void)time(&now);
#ifdef DEBUG
		printf("%s: now=%ld atime=%ld\n", devname,
		    (long)now, (long)st.st_atime);
#endif
		idle = now - st.st_atime;
		idle = (idle + 30) / 60; /* secs->mins */
	}
	if (idle < 0)
		idle = 0;

	return idle;
}
	
static int *
rusers_num_svc(void *arg, struct svc_req *rqstp)
{
	static int num_users = 0;
	struct utmp usr;
	FILE *ufp;

	ufp = fopen(_PATH_UTMP, "r");
	if (!ufp) {
		syslog(LOG_ERR, "%m");
		return (0);
	}

	/* only entries with both name and line fields */
	while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1)
		if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
			    sizeof(usr.ut_name))
#ifdef OSF
		    && usr.ut_type == USER_PROCESS
#endif
		    ) {
			num_users++;
		}

	fclose(ufp);
	return (&num_users);
}

static utmp_array *
do_names_3(int all)
{
	static utmp_array ut;
	struct utmp usr;
	int nusers = 0;
	FILE *ufp;

	memset(&ut, 0, sizeof(ut));
	ut.utmp_array_val = &utmps[0];
	
	ufp = fopen(_PATH_UTMP, "r");
	if (!ufp) {
		syslog(LOG_ERR, "%m");
		return (NULL);
	}

	/* only entries with both name and line fields */
	while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1 &&
	       nusers < MAXUSERS)
		if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
			    sizeof(usr.ut_name))
#ifdef OSF
		    && usr.ut_type == USER_PROCESS
#endif
		    ) {
			utmps[nusers].ut_type = RUSERS_USER_PROCESS;
			utmps[nusers].ut_time =
				usr.ut_time;
			utmps[nusers].ut_idle =
				getidle(usr.ut_line, usr.ut_host);
			utmps[nusers].ut_line = line[nusers];
			strncpy(line[nusers], usr.ut_line,
			    sizeof(line[nusers]));
			utmps[nusers].ut_user = name[nusers];
			strncpy(name[nusers], usr.ut_name,
			    sizeof(name[nusers]));
			utmps[nusers].ut_host = host[nusers];
			strncpy(host[nusers], usr.ut_host,
			    sizeof(host[nusers]));
			nusers++;
		}
	ut.utmp_array_len = nusers;

	fclose(ufp);
	return (&ut);
}

utmp_array *
rusersproc_names_3_svc(void *arg, struct svc_req *rqstp)
{

	return (do_names_3(0));
}

utmp_array *
rusersproc_allnames_3_svc(void *arg, struct svc_req *rqstp)
{

	return (do_names_3(1));
}

static struct utmpidlearr *
do_names_2(int all)
{
	static struct utmpidlearr ut;
	struct utmp usr;
	int nusers = 0;
	FILE *ufp;

	memset((char *)&ut, 0, sizeof(ut));
	ut.uia_arr = utmp_idlep;
	ut.uia_cnt = 0;
	
	ufp = fopen(_PATH_UTMP, "r");
	if (!ufp) {
		syslog(LOG_ERR, "%m");
		return (NULL);
	}

	/* only entries with both name and line fields */
	while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1 &&
	       nusers < MAXUSERS)
		if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
			    sizeof(usr.ut_name))
#ifdef OSF
		    && usr.ut_type == USER_PROCESS
#endif
		    ) {
			utmp_idlep[nusers] = &utmp_idle[nusers];
			utmp_idle[nusers].ui_utmp.ut_time =
				usr.ut_time;
			utmp_idle[nusers].ui_idle =
				getidle(usr.ut_line, usr.ut_host);
			strncpy(utmp_idle[nusers].ui_utmp.ut_line, usr.ut_line,
			    sizeof(utmp_idle[nusers].ui_utmp.ut_line));
			strncpy(utmp_idle[nusers].ui_utmp.ut_name, usr.ut_name,
			    sizeof(utmp_idle[nusers].ui_utmp.ut_name));
			strncpy(utmp_idle[nusers].ui_utmp.ut_host, usr.ut_host,
			    sizeof(utmp_idle[nusers].ui_utmp.ut_host));
			nusers++;
		}

	ut.uia_cnt = nusers;
	fclose(ufp);
	return (&ut);
}

struct utmpidlearr *
rusersproc_names_2_svc(void *arg, struct svc_req *rqstp)
{
	return (do_names_2(0));
}

struct utmpidlearr *
rusersproc_allnames_2_svc(void *arg, struct svc_req *rqstp)
{
	return (do_names_2(1));
}

void
rusers_service(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		int fill;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local) __P((void *, struct svc_req *));

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		goto leave;

	case RUSERSPROC_NUM:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_int;
		switch (rqstp->rq_vers) {
		case RUSERSVERS_3:
		case RUSERSVERS_IDLE:
			local = (char *(*) __P((void *, struct svc_req *)))
			    rusers_num_svc;
			break;
		default:
			svcerr_progvers(transp, RUSERSVERS_IDLE, RUSERSVERS_3);
			goto leave;
			/*NOTREACHED*/
		}
		break;

	case RUSERSPROC_NAMES:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_utmp_array;
		switch (rqstp->rq_vers) {
		case RUSERSVERS_3:
			local = (char *(*) __P((void *, struct svc_req *)))
			    rusersproc_names_3_svc;
			break;

		case RUSERSVERS_IDLE:
			xdr_result = (xdrproc_t)xdr_utmpidlearr;
			local = (char *(*) __P((void *, struct svc_req *)))
			    rusersproc_names_2_svc;
			break;

		default:
			svcerr_progvers(transp, RUSERSVERS_IDLE, RUSERSVERS_3);
			goto leave;
			/*NOTREACHED*/
		}
		break;

	case RUSERSPROC_ALLNAMES:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_utmp_array;
		switch (rqstp->rq_vers) {
		case RUSERSVERS_3:
			local = (char *(*) __P((void *, struct svc_req *)))
			    rusersproc_allnames_3_svc;
			break;

		case RUSERSVERS_IDLE:
			xdr_result = (xdrproc_t)xdr_utmpidlearr;
			local = (char *(*) __P((void *, struct svc_req *)))
			    rusersproc_allnames_2_svc;
			break;

		default:
			svcerr_progvers(transp, RUSERSVERS_IDLE, RUSERSVERS_3);
			goto leave;
			/*NOTREACHED*/
		}
		break;

	default:
		svcerr_noproc(transp);
		goto leave;
	}
	memset((char *)&argument, 0, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		goto leave;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
leave:
	if (from_inetd)
		exit(0);
}

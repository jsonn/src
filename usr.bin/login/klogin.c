/*	$NetBSD: klogin.c,v 1.14.6.1 2000/06/23 16:30:34 minoura Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)klogin.c	8.3 (Berkeley) 4/2/94";
#endif
__RCSID("$NetBSD: klogin.c,v 1.14.6.1 2000/06/23 16:30:34 minoura Exp $");
#endif /* not lint */

#ifdef KERBEROS
#include <sys/param.h>
#include <sys/syslog.h>
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>

#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define	INITIAL_TICKET	"krbtgt"
#define	VERIFY_SERVICE	"rcmd"

int notickets;
char *krbtkfile_env;
char *tty;
extern int has_ccache;

static char tkt_location[MAXPATHLEN];  /* a pointer to this is returned... */

int  klogin __P((struct passwd *, char *, char *, char *));
void kdestroy __P((void));

/*
 * Attempt to log the user in using Kerberos authentication
 *
 * return 0 on success (will be logged in)
 *	  1 if Kerberos failed (try local password in login)
 */
int
klogin(pw, instance, localhost, password)
	struct passwd *pw;
	char *instance, *localhost, *password;
{
	int kerror;
	AUTH_DAT authdata;
	KTEXT_ST ticket;
	struct hostent *hp;
	unsigned long faddr;
	char realm[REALM_SZ], savehost[MAXHOSTNAMELEN];

	/*
	 * Root logins don't use Kerberos.
	 * If we have a realm, try getting a ticket-granting ticket
	 * and using it to authenticate.  Otherwise, return
	 * failure so that we can try the normal passwd file
	 * for a password.  If that's ok, log the user in
	 * without issuing any tickets.
	 */
	if (strcmp(pw->pw_name, "root") == 0 ||
	    krb_get_lrealm(realm, 0) != KSUCCESS)
		return (1);

	/*
	 * get TGT for local realm
	 * tickets are stored in a file named TKT_ROOT plus uid plus tty
	 * except for user.root tickets.
	 */

	if (strcmp(instance, "root") != 0)
		(void)snprintf(tkt_location, sizeof tkt_location, "%s%d.%s",
		    TKT_ROOT, pw->pw_uid, tty);
	else
		(void)snprintf(tkt_location, sizeof tkt_location,
		    "%s_root_%d.%s", TKT_ROOT, pw->pw_uid, tty);
	krbtkfile_env = tkt_location;
	has_ccache = 1;
	(void)krb_set_tkt_string(tkt_location);

	/*
	 * Set real as well as effective ID to 0 for the moment,
	 * to make the kerberos library do the right thing.
	 */
	if (setuid(0) < 0) {
		warnx("setuid");
		return (1);
	}
	kerror = krb_get_pw_in_tkt(pw->pw_name, instance,
		    realm, INITIAL_TICKET, realm, DEFAULT_TKT_LIFE, password);
	/*
	 * If we got a TGT, get a local "rcmd" ticket and check it so as to
	 * ensure that we are not talking to a bogus Kerberos server.
	 *
	 * There are 2 cases where we still allow a login:
	 *	1: the VERIFY_SERVICE doesn't exist in the KDC
	 *	2: local host has no srvtab, as (hopefully) indicated by a
	 *	   return value of RD_AP_UNDEC from krb_rd_req().
	 */
	if (kerror != INTK_OK) {
		if (kerror != INTK_BADPW && kerror != KDC_PR_UNKNOWN) {
			syslog(LOG_ERR, "Kerberos intkt error: %s",
			    krb_err_txt[kerror]);
			dest_tkt();
		}
		return (1);
	}

	if (chown(TKT_FILE, pw->pw_uid, pw->pw_gid) < 0)
		syslog(LOG_ERR, "chown tkfile (%s): %m", TKT_FILE);

	(void)strncpy(savehost, krb_get_phost(localhost), sizeof(savehost));
	savehost[sizeof(savehost)-1] = NULL;

	/*
	 * if the "VERIFY_SERVICE" doesn't exist in the KDC for this host,
	 * still allow login with tickets, but log the error condition.
	 */

	kerror = krb_mk_req(&ticket, VERIFY_SERVICE, savehost, realm, 33);
	if (kerror == KDC_PR_UNKNOWN) {
		syslog(LOG_NOTICE,
    		    "warning: TGT not verified (%s); %s.%s not registered, or srvtab is wrong?",
		    krb_err_txt[kerror], VERIFY_SERVICE, savehost);
		notickets = 0;
		/*
		 * but for security, don't allow root instances in under
		 * this condition!
		 */
		if (strcmp(instance, "root") == 0) {
		  syslog(LOG_ERR, "Kerberos %s root instance login refused\n",
			 pw->pw_name);
		  dest_tkt();
		  return (1);
		}
		return (0);
	}

	if (kerror != KSUCCESS) {
		warnx("unable to use TGT: (%s)", krb_err_txt[kerror]);
		syslog(LOG_NOTICE, "unable to use TGT: (%s)",
		    krb_err_txt[kerror]);
		dest_tkt();
		return (1);
	}

	if (!(hp = gethostbyname(localhost))) {
		syslog(LOG_ERR, "couldn't get local host address");
		dest_tkt();
		return (1);
	}

	memmove((void *)&faddr, (void *)hp->h_addr, sizeof(faddr));

	kerror = krb_rd_req(&ticket, VERIFY_SERVICE, savehost, faddr,
	    &authdata, "");

	if (kerror == KSUCCESS) {
		notickets = 0;
		return (0);
	}

	/* undecipherable: probably didn't have a srvtab on the local host */
	if (kerror == RD_AP_UNDEC) {
		syslog(LOG_NOTICE, "krb_rd_req: (%s)\n", krb_err_txt[kerror]);
		dest_tkt();
		return (1);
	}
	/* failed for some other reason */
	warnx("unable to verify %s ticket: (%s)", VERIFY_SERVICE,
	    krb_err_txt[kerror]);
	syslog(LOG_NOTICE, "couldn't verify %s ticket: %s", VERIFY_SERVICE,
	    krb_err_txt[kerror]);
	dest_tkt();
	return (1);
}

void
kdestroy()
{
        char *file = krbtkfile_env;
	int i, fd;
	extern int errno;
	struct stat statb;
	char buf[BUFSIZ];
#ifdef TKT_SHMEM
	char shmidname[MAXPATHLEN];
#endif /* TKT_SHMEM */

	if (krbtkfile_env == NULL)
	    return;

	errno = 0;
	if (lstat(file, &statb) < 0)
	    goto out;

	if (!S_ISREG(statb.st_mode)
#ifdef notdef
	    || statb.st_mode & 077
#endif
	    )
		goto out;

	if ((fd = open(file, O_RDWR, 0)) < 0)
	    goto out;

	memset(buf, 0, BUFSIZ);

	for (i = 0; i < statb.st_size; i += BUFSIZ)
	    if (write(fd, buf, BUFSIZ) != BUFSIZ) {
		(void) fsync(fd);
		(void) close(fd);
		goto out;
	    }

	(void) fsync(fd);
	(void) close(fd);

	(void) unlink(file);

out:
	if (errno != 0)
		return;
#ifdef TKT_SHMEM
	/* 
	 * handle the shared memory case 
	 */
	/* 5 == 4 (".shm") + 1 */
	(void)strncpy(shmidname, file, sizeof(shmidname) - 5);
	(void)strcat(shmidname, ".shm");	/* XXX strcat is safe */
	if (krb_shm_dest(shmidname) != KSUCCESS)
	    return;
#endif /* TKT_SHMEM */
	return;
}
#endif

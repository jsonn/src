/*	$NetBSD: util.c,v 1.95.2.1 2000/06/23 16:30:25 minoura Exp $	*/

/*-
 * Copyright (c) 1997-2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1985, 1989, 1993, 1994
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
__RCSID("$NetBSD: util.c,v 1.95.2.1 2000/06/23 16:30:25 minoura Exp $");
#endif /* not lint */

/*
 * FTP User Program -- Misc support routines
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <signal.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#ifdef INET6
#include <netdb.h>
#endif

#include "ftp_var.h"

/*
 * Connect to peer server and auto-login, if possible.
 */
void
setpeer(int argc, char *argv[])
{
	char *host;
	char *port;

	if (argc == 0)
		goto usage;
	if (connected) {
		fprintf(ttyout, "Already connected to %s, use close first.\n",
		    hostname);
		code = -1;
		return;
	}
	if (argc < 2)
		(void)another(&argc, &argv, "to");
	if (argc < 2 || argc > 3) {
 usage:
		fprintf(ttyout, "usage: %s host-name [port]\n", argv[0]);
		code = -1;
		return;
	}
	if (gatemode)
		port = gateport;
	else
		port = ftpport;
	if (argc > 2)
		port = argv[2];

	if (gatemode) {
		if (gateserver == NULL || *gateserver == '\0')
			errx(1, "gateserver not defined (shouldn't happen)");
		host = hookup(gateserver, port);
	} else
		host = hookup(argv[1], port);

	if (host) {
		if (gatemode && verbose) {
			fprintf(ttyout,
			    "Connecting via pass-through server %s\n",
			    gateserver);
		}

		connected = 1;
		/*
		 * Set up defaults for FTP.
		 */
		(void)strlcpy(typename, "ascii", sizeof(typename));
		type = TYPE_A;
		curtype = TYPE_A;
		(void)strlcpy(formname, "non-print", sizeof(formname));
		form = FORM_N;
		(void)strlcpy(modename, "stream", sizeof(modename));
		mode = MODE_S;
		(void)strlcpy(structname, "file", sizeof(structname));
		stru = STRU_F;
		(void)strlcpy(bytename, "8", sizeof(bytename));
		bytesize = 8;
		if (autologin)
			(void)ftp_login(argv[1], NULL, NULL);
	}
}

/*
 * Determine the remote system type.
 * Call after a successful login (i.e, connected = -1)
 */
void
remotesyst(void)
{
	int overbose;

	overbose = verbose;
	if (debug == 0)
		verbose = -1;
	if (command("SYST") == COMPLETE && overbose) {
		char *cp, c;
		c = 0;
		cp = strchr(reply_string + 4, ' ');
		if (cp == NULL)
			cp = strchr(reply_string + 4, '\r');
		if (cp) {
			if (cp[-1] == '.')
				cp--;
			c = *cp;
			*cp = '\0';
		}

		fprintf(ttyout, "Remote system type is %s.\n",
		    reply_string + 4);
		if (cp)
			*cp = c;
	}
	if (!strncmp(reply_string, "215 UNIX Type: L8", 17)) {
		if (proxy)
			unix_proxy = 1;
		else
			unix_server = 1;
		/*
		 * Set type to 0 (not specified by user),
		 * meaning binary by default, but don't bother
		 * telling server.  We can use binary
		 * for text files unless changed by the user.
		 */
		type = 0;
		(void)strlcpy(typename, "binary", sizeof(typename));
		if (overbose)
		    fprintf(ttyout,
			"Using %s mode to transfer files.\n",
			typename);
	} else {
		if (proxy)
			unix_proxy = 0;
		else
			unix_server = 0;
		if (overbose &&
		    !strncmp(reply_string, "215 TOPS20", 10))
			fputs(
"Remember to set tenex mode when transferring binary files from this machine.\n",
			    ttyout);
	}
	verbose = overbose;
}

/*
 * Reset the various variables that indicate connection state back to
 * disconnected settings.
 * The caller is responsible for issuing any commands to the remote server
 * to perform a clean shutdown before this is invoked.
 */
void
cleanuppeer(void)
{

	if (cout)
		(void)fclose(cout);
	cout = NULL;
	connected = 0;
	unix_server = 0;
	unix_proxy = 0;
			/*
			 * determine if anonftp was specifically set with -a
			 * (1), or implicitly set by auto_fetch() (2). in the
			 * latter case, disable after the current xfer
			 */
	if (anonftp == 2)
		anonftp = 0;
	data = -1;
	epsv4bad = 0;
	if (username)
		free(username);
	username = NULL;
	if (!proxy)
		macnum = 0;
}

/*
 * Top-level signal handler for interrupted commands.
 */
void
intr(int dummy)
{

	alarmtimer(0);
	if (fromatty)
		write(fileno(ttyout), "\n", 1);
	siglongjmp(toplevel, 1);
}

/*
 * Signal handler for lost connections; cleanup various elements of
 * the connection state, and call cleanuppeer() to finish it off.
 */
void
lostpeer(int dummy)
{
	int oerrno = errno;

	alarmtimer(0);
	if (connected) {
		if (cout != NULL) {
			(void)shutdown(fileno(cout), 1+1);
			(void)fclose(cout);
			cout = NULL;
		}
		if (data >= 0) {
			(void)shutdown(data, 1+1);
			(void)close(data);
			data = -1;
		}
		connected = 0;
	}
	pswitch(1);
	if (connected) {
		if (cout != NULL) {
			(void)shutdown(fileno(cout), 1+1);
			(void)fclose(cout);
			cout = NULL;
		}
		connected = 0;
	}
	proxflag = 0;
	pswitch(0);
	cleanuppeer();
	errno = oerrno;
}


/*
 * Login to remote host, using given username & password if supplied.
 * Return non-zero if successful.
 */
int
ftp_login(const char *host, const char *user, const char *pass)
{
	char tmp[80];
	const char *acct;
	struct passwd *pw;
	int n, aflag, rval, freeuser, freepass, freeacct;

	acct = NULL;
	aflag = rval = freeuser = freepass = freeacct = 0;

	if (debug)
		fprintf(ttyout, "ftp_login: user `%s' pass `%s' host `%s'\n",
		    user ? user : "<null>", pass ? pass : "<null>",
		    host ? host : "<null>");


	/*
	 * Set up arguments for an anonymous FTP session, if necessary.
	 */
	if (anonftp) {
		user = "anonymous";	/* as per RFC 1635 */
		pass = getoptionvalue("anonpass");
	}

	if (user == NULL)
		freeuser = 1;
	if (pass == NULL)
		freepass = 1;
	freeacct = 1;
	if (ruserpass(host, &user, &pass, &acct) < 0) {
		code = -1;
		goto cleanup_ftp_login;
	}

	while (user == NULL) {
		const char *myname = getlogin();

		if (myname == NULL && (pw = getpwuid(getuid())) != NULL)
			myname = pw->pw_name;
		if (myname)
			fprintf(ttyout, "Name (%s:%s): ", host, myname);
		else
			fprintf(ttyout, "Name (%s): ", host);
		*tmp = '\0';
		if (fgets(tmp, sizeof(tmp) - 1, stdin) == NULL) {
			fprintf(ttyout, "\nEOF received; login aborted.\n");
			clearerr(stdin);
			code = -1;
			goto cleanup_ftp_login;
		}
		tmp[strlen(tmp) - 1] = '\0';
		freeuser = 0;
		if (*tmp == '\0')
			user = myname;
		else
			user = tmp;
	}

	if (gatemode) {
		char *nuser;
		int len;

		len = strlen(user) + 1 + strlen(host) + 1;
		nuser = xmalloc(len);
		(void)strlcpy(nuser, user, len);
		(void)strlcat(nuser, "@",  len);
		(void)strlcat(nuser, host, len);
		freeuser = 1;
		user = nuser;
	}

	n = command("USER %s", user);
	if (n == CONTINUE) {
		if (pass == NULL) {
			freepass = 0;
			pass = getpass("Password:");
		}
		n = command("PASS %s", pass);
	}
	if (n == CONTINUE) {
		aflag++;
		if (acct == NULL) {
			freeacct = 0;
			acct = getpass("Account:");
		}
		if (acct[0] == '\0') {
			warnx("Login failed.");
			goto cleanup_ftp_login;
		}
		n = command("ACCT %s", acct);
	}
	if ((n != COMPLETE) ||
	    (!aflag && acct != NULL && command("ACCT %s", acct) != COMPLETE)) {
		warnx("Login failed.");
		goto cleanup_ftp_login;
	}
	rval = 1;
	username = xstrdup(user);
	if (proxy)
		goto cleanup_ftp_login;

	connected = -1;
	remotesyst();
	for (n = 0; n < macnum; ++n) {
		if (!strcmp("init", macros[n].mac_name)) {
			(void)strlcpy(line, "$init", sizeof(line));
			makeargv();
			domacro(margc, margv);
			break;
		}
	}
	updateremotepwd();

cleanup_ftp_login:
	if (user != NULL && freeuser)
		free((char *)user);
	if (pass != NULL && freepass)
		free((char *)pass);
	if (acct != NULL && freeacct)
		free((char *)acct);
	return (rval);
}

/*
 * `another' gets another argument, and stores the new argc and argv.
 * It reverts to the top level (via intr()) on EOF/error.
 *
 * Returns false if no new arguments have been added.
 */
int
another(int *pargc, char ***pargv, const char *prompt)
{
	int len = strlen(line), ret;

	if (len >= sizeof(line) - 3) {
		fputs("sorry, arguments too long.\n", ttyout);
		intr(0);
	}
	fprintf(ttyout, "(%s) ", prompt);
	line[len++] = ' ';
	if (fgets(&line[len], sizeof(line) - len, stdin) == NULL) {
		clearerr(stdin);
		intr(0);
	}
	len += strlen(&line[len]);
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';
	makeargv();
	ret = margc > *pargc;
	*pargc = margc;
	*pargv = margv;
	return (ret);
}

/*
 * glob files given in argv[] from the remote server.
 * if errbuf isn't NULL, store error messages there instead
 * of writing to the screen.
 */
char *
remglob(char *argv[], int doswitch, char **errbuf)
{
        char temp[MAXPATHLEN];
        static char buf[MAXPATHLEN];
        static FILE *ftemp = NULL;
        static char **args;
        int oldverbose, oldhash, fd, len;
        char *cp, *mode;

        if (!mflag || !connected) {
                if (!doglob)
                        args = NULL;
                else {
                        if (ftemp) {
                                (void)fclose(ftemp);
                                ftemp = NULL;
                        }
                }
                return (NULL);
        }
        if (!doglob) {
                if (args == NULL)
                        args = argv;
                if ((cp = *++args) == NULL)
                        args = NULL;
                return (cp);
        }
        if (ftemp == NULL) {
		len = strlcpy(temp, tmpdir, sizeof(temp));
		if (temp[len - 1] != '/')
			(void)strlcat(temp, "/", sizeof(temp));
		(void)strlcat(temp, TMPFILE, sizeof(temp));
                if ((fd = mkstemp(temp)) < 0) {
                        warn("unable to create temporary file %s", temp);
                        return (NULL);
                }
                close(fd);
                oldverbose = verbose;
		verbose = (errbuf != NULL) ? -1 : 0;
                oldhash = hash;
                hash = 0;
                if (doswitch)
                        pswitch(!proxy);
                for (mode = "w"; *++argv != NULL; mode = "a")
                        recvrequest("NLST", temp, *argv, mode, 0, 0);
		if ((code / 100) != COMPLETE) {
			if (errbuf != NULL)
				*errbuf = reply_string;
		}
                if (doswitch)
                        pswitch(!proxy);
                verbose = oldverbose;
		hash = oldhash;
                ftemp = fopen(temp, "r");
                (void)unlink(temp);
                if (ftemp == NULL) {
			if (errbuf == NULL)
				fputs(
				    "can't find list of remote files, oops.\n",
				    ttyout);
			else
				*errbuf =
				    "can't find list of remote files, oops.";
                        return (NULL);
                }
        }
        if (fgets(buf, sizeof(buf), ftemp) == NULL) {
                (void)fclose(ftemp);
		ftemp = NULL;
                return (NULL);
        }
        if ((cp = strchr(buf, '\n')) != NULL)
                *cp = '\0';
        return (buf);
}

/*
 * Glob a local file name specification with the expectation of a single
 * return value. Can't control multiple values being expanded from the
 * expression, we return only the first.
 * Returns NULL on error, or a pointer to a buffer containing the filename
 * that's the caller's responsiblity to free(3) when finished with.
 */
char *
globulize(const char *pattern)
{
	glob_t gl;
	int flags;
	char *p;

	if (!doglob)
		return (xstrdup(pattern));

	flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_TILDE;
	memset(&gl, 0, sizeof(gl));
	if (glob(pattern, flags, NULL, &gl) || gl.gl_pathc == 0) {
		warnx("%s: not found", pattern);
		globfree(&gl);
		return (NULL);
	}
	p = xstrdup(gl.gl_pathv[0]);
	globfree(&gl);
	return (p);
}

/*
 * determine size of remote file
 */
off_t
remotesize(const char *file, int noisy)
{
	int overbose;
	off_t size;

	overbose = verbose;
	size = -1;
	if (debug == 0)
		verbose = -1;
	if (command("SIZE %s", file) == COMPLETE) {
		char *cp, *ep;

		cp = strchr(reply_string, ' ');
		if (cp != NULL) {
			cp++;
#ifndef NO_QUAD
			size = strtoll(cp, &ep, 10);
#else
			size = strtol(cp, &ep, 10);
#endif
			if (*ep != '\0' && !isspace((unsigned char)*ep))
				size = -1;
		}
	} else if (noisy && debug == 0) {
		fputs(reply_string, ttyout);
		putc('\n', ttyout);
	}
	verbose = overbose;
	return (size);
}

/*
 * determine last modification time (in GMT) of remote file
 */
time_t
remotemodtime(const char *file, int noisy)
{
	int overbose;
	time_t rtime;
	int ocode;

	overbose = verbose;
	ocode = code;
	rtime = -1;
	if (debug == 0)
		verbose = -1;
	if (command("MDTM %s", file) == COMPLETE) {
		struct tm timebuf;
		char *timestr, *frac;
		int yy, mo, day, hour, min, sec;

		/*
		 * time-val = 14DIGIT [ "." 1*DIGIT ]
		 *		YYYYMMDDHHMMSS[.sss]
		 * mdtm-response = "213" SP time-val CRLF / error-response
		 */
		timestr = reply_string + 4;

					/*
					 * parse fraction.
					 * XXX: ignored for now
					 */
		frac = strchr(timestr, '\r');
		if (frac != NULL)
			*frac = '\0';
		frac = strchr(timestr, '.');
		if (frac != NULL)
			*frac++ = '\0';
		if (strlen(timestr) == 15 && strncmp(timestr, "191", 3) == 0) {
			/*
			 * XXX:	Workaround for lame ftpd's that return
			 *	`19100' instead of `2000'
			 */
			fprintf(ttyout,
	    "Y2K warning! Incorrect time-val `%s' received from server.\n",
			    timestr);
			timestr++;
			timestr[0] = '2';
			timestr[1] = '0';
			fprintf(ttyout, "Converted to `%s'\n", timestr);
		}
		if (strlen(timestr) != 14 ||
		    sscanf(timestr, "%04d%02d%02d%02d%02d%02d",
			&yy, &mo, &day, &hour, &min, &sec) != 6) {
 bad_parse_time:
			fprintf(ttyout, "Can't parse time `%s'.\n", timestr);
			goto cleanup_parse_time;
		}
		memset(&timebuf, 0, sizeof(timebuf));
		timebuf.tm_sec = sec;
		timebuf.tm_min = min;
		timebuf.tm_hour = hour;
		timebuf.tm_mday = day;
		timebuf.tm_mon = mo - 1;
		timebuf.tm_year = yy - TM_YEAR_BASE; 
		timebuf.tm_isdst = -1;
		rtime = timegm(&timebuf);
		if (rtime == -1) {
			if (noisy || debug != 0)
				goto bad_parse_time;
			else
				goto cleanup_parse_time;
		} else if (debug)
			fprintf(ttyout, "parsed date as: %s", ctime(&rtime));
	} else if (noisy && debug == 0) {
		fputs(reply_string, ttyout);
		putc('\n', ttyout);
	}
 cleanup_parse_time:
	verbose = overbose;
	if (rtime == -1)
		code = ocode;
	return (rtime);
}

/*
 * update global `remotepwd', which contains the state of the remote cwd
 */
void
updateremotepwd(void)
{
	int	 overbose, ocode, i;
	char	*cp;

	overbose = verbose;
	ocode = code;
	if (debug == 0)
		verbose = -1;
	if (command("PWD") != COMPLETE)
		goto badremotepwd;
	cp = strchr(reply_string, ' ');
	if (cp == NULL || cp[0] == '\0' || cp[1] != '"')
		goto badremotepwd;
	cp += 2;
	for (i = 0; *cp && i < sizeof(remotepwd) - 1; i++, cp++) {
		if (cp[0] == '"') {
			if (cp[1] == '"')
				cp++;
			else
				break;
		}
		remotepwd[i] = *cp;
	}
	remotepwd[i] = '\0';
	if (debug)
		fprintf(ttyout, "got remotepwd as `%s'\n", remotepwd);
	goto cleanupremotepwd;
 badremotepwd:
	remotepwd[0]='\0';
 cleanupremotepwd:
	verbose = overbose;
	code = ocode;
}

#ifndef	NO_PROGRESS

/*
 * return non-zero if we're the current foreground process
 */
int
foregroundproc(void)
{
	static pid_t pgrp = -1;

	if (pgrp == -1)
		pgrp = getpgrp();

	return (tcgetpgrp(fileno(ttyout)) == pgrp);
}


static void updateprogressmeter(int);

/*
 * SIGALRM handler to update the progress meter
 */
static void
updateprogressmeter(int dummy)
{
	int oerrno = errno;

	progressmeter(0);
	errno = oerrno;
}
#endif	/* NO_PROGRESS */


/*
 * List of order of magnitude prefixes.
 * The last is `P', as 2^64 = 16384 Petabytes
 */
static const char prefixes[] = " KMGTP";

/*
 * Display a transfer progress bar if progress is non-zero.
 * SIGALRM is hijacked for use by this function.
 * - Before the transfer, set filesize to size of file (or -1 if unknown),
 *   and call with flag = -1. This starts the once per second timer,
 *   and a call to updateprogressmeter() upon SIGALRM.
 * - During the transfer, updateprogressmeter will call progressmeter
 *   with flag = 0
 * - After the transfer, call with flag = 1
 */
static struct timeval start;
static struct timeval lastupdate;

#define	BUFLEFT	(sizeof(buf) - len)

void
progressmeter(int flag)
{
	static off_t lastsize;
#ifndef NO_PROGRESS
	struct timeval now, td, wait;
	off_t cursize, abbrevsize, bytespersec;
	double elapsed;
	int ratio, barlength, i, len, remaining;

			/*
			 * Work variables for progress bar.
			 *
			 * XXX:	if the format of the progress bar changes
			 *	(especially the number of characters in the
			 *	`static' portion of it), be sure to update
			 *	these appropriately.
			 */
	char		buf[256];	/* workspace for progress bar */
#define	BAROVERHEAD	43		/* non `*' portion of progress bar */
					/*
					 * stars should contain at least
					 * sizeof(buf) - BAROVERHEAD entries
					 */
	const char	stars[] =
"*****************************************************************************"
"*****************************************************************************"
"*****************************************************************************";

#endif

	if (flag == -1) {
		(void)gettimeofday(&start, NULL);
		lastupdate = start;
		lastsize = restart_point;
	}
#ifndef NO_PROGRESS
	len = 0;
	if (!progress || filesize <= 0)
		return;

	(void)gettimeofday(&now, NULL);
	cursize = bytes + restart_point;
	timersub(&now, &lastupdate, &wait);
	if (cursize > lastsize) {
		lastupdate = now;
		lastsize = cursize;
		wait.tv_sec = 0;
	}

	/*
	 * print progress bar only if we are foreground process.
	 */
	if (! foregroundproc())
		return;

	ratio = (int)((double)cursize * 100.0 / (double)filesize);
	ratio = MAX(ratio, 0);
	ratio = MIN(ratio, 100);
	len += snprintf(buf + len, BUFLEFT, "\r%3d%% ", ratio);

			/*
			 * calculate the length of the `*' bar, ensuring that
			 * the number of stars won't exceed the buffer size 
			 */
	barlength = MIN(sizeof(buf) - 1, ttywidth) - BAROVERHEAD;
	if (barlength > 0) {
		i = barlength * ratio / 100;
		len += snprintf(buf + len, BUFLEFT,
		    "|%.*s%*s|", i, stars, barlength - i, "");
	}

	abbrevsize = cursize;
	for (i = 0; abbrevsize >= 100000 && i < sizeof(prefixes); i++)
		abbrevsize >>= 10;
	len += snprintf(buf + len, BUFLEFT,
#ifndef NO_QUAD
	    " %5lld %c%c ", (long long)abbrevsize,
#else
	    " %5ld %c%c ", (long)abbrevsize,
#endif
	    prefixes[i],
	    i == 0 ? ' ' : 'B');

	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);

	bytespersec = 0;
	if (bytes > 0) {
		bytespersec = bytes;
		if (elapsed > 0.0)
			bytespersec /= elapsed;
	}
	for (i = 1; bytespersec >= 1024000 && i < sizeof(prefixes); i++)
		bytespersec >>= 10;
	len += snprintf(buf + len, BUFLEFT,
#ifndef NO_QUAD
	    " %3lld.%02d %cB/s ", (long long)bytespersec / 1024,
#else
	    " %3ld.%02d %cB/s ", (long)bytespersec / 1024,
#endif
	    (int)((bytespersec % 1024) * 100 / 1024),
	    prefixes[i]);

	if (bytes <= 0 || elapsed <= 0.0 || cursize > filesize) {
		len += snprintf(buf + len, BUFLEFT, "   --:-- ETA");
	} else if (wait.tv_sec >= STALLTIME) {
		len += snprintf(buf + len, BUFLEFT, " - stalled -");
	} else {
		remaining = (int)
		    ((filesize - restart_point) / (bytes / elapsed) - elapsed);
		if (remaining >= 100 * SECSPERHOUR)
			len += snprintf(buf + len, BUFLEFT, "   --:-- ETA");
		else {
			i = remaining / SECSPERHOUR;
			if (i)
				len += snprintf(buf + len, BUFLEFT, "%2d:", i);
			else
				len += snprintf(buf + len, BUFLEFT, "   ");
			i = remaining % SECSPERHOUR;
			len += snprintf(buf + len, BUFLEFT,
			    "%02d:%02d ETA", i / 60, i % 60);
		}
	}
	if (flag == 1)
		len += snprintf(buf + len, BUFLEFT, "\n");
	(void)write(fileno(ttyout), buf, len);

	if (flag == -1) {
		(void)xsignal_restart(SIGALRM, updateprogressmeter, 1);
		alarmtimer(1);		/* set alarm timer for 1 Hz */
	} else if (flag == 1) {
		(void)xsignal(SIGALRM, SIG_DFL);
		alarmtimer(0);
	}
#endif	/* !NO_PROGRESS */
}

/*
 * Display transfer statistics.
 * Requires start to be initialised by progressmeter(-1),
 * direction to be defined by xfer routines, and filesize and bytes
 * to be updated by xfer routines
 * If siginfo is nonzero, an ETA is displayed, and the output goes to stderr
 * instead of ttyout.
 */
void
ptransfer(int siginfo)
{
	struct timeval now, td, wait;
	double elapsed;
	off_t bytespersec;
	int remaining, hh, i, len;

	char buf[256];		/* Work variable for transfer status. */

	if (!verbose && !progress && !siginfo)
		return;

	(void)gettimeofday(&now, NULL);
	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);
	bytespersec = 0;
	if (bytes > 0) {
		bytespersec = bytes;
		if (elapsed > 0.0)
			bytespersec /= elapsed;
	}
	len = 0;
	len += snprintf(buf + len, BUFLEFT,
#ifndef NO_QUAD
	    "%lld byte%s %s in ", (long long)bytes,
#else
	    "%ld byte%s %s in ", (long)bytes,
#endif
	    bytes == 1 ? "" : "s", direction);
	remaining = (int)elapsed;
	if (remaining > SECSPERDAY) {
		int days;

		days = remaining / SECSPERDAY;
		remaining %= SECSPERDAY;
		len += snprintf(buf + len, BUFLEFT,
		    "%d day%s ", days, days == 1 ? "" : "s");
	}
	hh = remaining / SECSPERHOUR;
	remaining %= SECSPERHOUR;
	if (hh)
		len += snprintf(buf + len, BUFLEFT, "%2d:", hh);
	len += snprintf(buf + len, BUFLEFT,
	    "%02d:%02d ", remaining / 60, remaining % 60);

	for (i = 1; bytespersec >= 1024000 && i < sizeof(prefixes); i++)
		bytespersec >>= 10;
	len += snprintf(buf + len, BUFLEFT,
#ifndef NO_QUAD
	    "(%lld.%02d %cB/s)", (long long)bytespersec / 1024,
#else
	    "(%ld.%02d %cB/s)", (long)bytespersec / 1024,
#endif
	    (int)((bytespersec % 1024) * 100 / 1024),
	    prefixes[i]);

	if (siginfo && bytes > 0 && elapsed > 0.0 && filesize >= 0
	    && bytes + restart_point <= filesize) {
		remaining = (int)((filesize - restart_point) /
				  (bytes / elapsed) - elapsed);
		hh = remaining / SECSPERHOUR;
		remaining %= SECSPERHOUR;
		len += snprintf(buf + len, BUFLEFT, "  ETA: ");
		if (hh)
			len += snprintf(buf + len, BUFLEFT, "%2d:", hh);
		len += snprintf(buf + len, BUFLEFT, "%02d:%02d",
		    remaining / 60, remaining % 60);
		timersub(&now, &lastupdate, &wait);
		if (wait.tv_sec >= STALLTIME)
			len += snprintf(buf + len, BUFLEFT, "  (stalled)");
	}
	len += snprintf(buf + len, BUFLEFT, "\n");
	(void)write(siginfo ? STDERR_FILENO : fileno(ttyout), buf, len);
}

/*
 * SIG{INFO,QUIT} handler to print transfer stats if a transfer is in progress
 */
void
psummary(int notused)
{
	int oerrno = errno;

	if (bytes > 0) {
		if (fromatty)
			write(fileno(ttyout), "\n", 1);
		ptransfer(1);
	}
	errno = oerrno;
}

/*
 * List words in stringlist, vertically arranged
 */
void
list_vertical(StringList *sl)
{
	int i, j, w;
	int columns, width, lines, items;
	char *p;

	width = items = 0;

	for (i = 0 ; i < sl->sl_cur ; i++) {
		w = strlen(sl->sl_str[i]);
		if (w > width)
			width = w;
	}
	width = (width + 8) &~ 7;

	columns = ttywidth / width;
	if (columns == 0)
		columns = 1;
	lines = (sl->sl_cur + columns - 1) / columns;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < columns; j++) {
			p = sl->sl_str[j * lines + i];
			if (p)
				fputs(p, ttyout);
			if (j * lines + i + lines >= sl->sl_cur) {
				putc('\n', ttyout);
				break;
			}
			w = strlen(p);
			while (w < width) {
				w = (w + 8) &~ 7;
				(void)putc('\t', ttyout);
			}
		}
	}
}

/*
 * Update the global ttywidth value, using TIOCGWINSZ.
 */
void
setttywidth(int a)
{
	struct winsize winsize;
	int oerrno = errno;

	if (ioctl(fileno(ttyout), TIOCGWINSZ, &winsize) != -1 &&
	    winsize.ws_col != 0)
		ttywidth = winsize.ws_col;
	else
		ttywidth = 80;
	errno = oerrno;
}

/*
 * Change the rate limit up (SIGUSR1) or down (SIGUSR2)
 */
void
crankrate(int sig)
{

	switch (sig) {
	case SIGUSR1:
		if (rate_get)
			rate_get += rate_get_incr;
		if (rate_put)
			rate_put += rate_put_incr;
		break;
	case SIGUSR2:
		if (rate_get && rate_get > rate_get_incr)
			rate_get -= rate_get_incr;
		if (rate_put && rate_put > rate_put_incr)
			rate_put -= rate_put_incr;
		break;
	default:
		err(1, "crankrate invoked with unknown signal: %d", sig);
	}
}


/*
 * Set the SIGALRM interval timer for wait seconds, 0 to disable.
 */
void
alarmtimer(int wait)
{
	struct itimerval itv;

	itv.it_value.tv_sec = wait;
	itv.it_value.tv_usec = 0;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

/*
 * Setup or cleanup EditLine structures
 */
#ifndef NO_EDITCOMPLETE
void
controlediting(void)
{
	if (editing && el == NULL && hist == NULL) {
		HistEvent ev;
		int editmode;

		el = el_init(__progname, stdin, ttyout, stderr);
		/* init editline */
		hist = history_init();		/* init the builtin history */
		history(hist, &ev, H_SETSIZE, 100);/* remember 100 events */
		el_set(el, EL_HIST, history, hist);	/* use history */

		el_set(el, EL_EDITOR, "emacs");	/* default editor is emacs */
		el_set(el, EL_PROMPT, prompt);	/* set the prompt functions */
		el_set(el, EL_RPROMPT, rprompt);

		/* add local file completion, bind to TAB */
		el_set(el, EL_ADDFN, "ftp-complete",
		    "Context sensitive argument completion",
		    complete);
		el_set(el, EL_BIND, "^I", "ftp-complete", NULL);
		el_source(el, NULL);	/* read ~/.editrc */
		if ((el_get(el, EL_EDITMODE, &editmode) != -1) && editmode == 0)
			editing = 0;	/* the user doesn't want editing,
					 * so disable, and let statement
					 * below cleanup */
		else
			el_set(el, EL_SIGNAL, 1);
	}
	if (!editing) {
		if (hist) {
			history_end(hist);
			hist = NULL;
		}
		if (el) {
			el_end(el);
			el = NULL;
		}
	}
}
#endif /* !NO_EDITCOMPLETE */

/*
 * Convert the string `arg' to an int, which may have an optional SI suffix
 * (`b', `k', `m', `g'). Returns the number for success, -1 otherwise.
 */
int
strsuftoi(const char *arg)
{
	char *cp;
	long val;

	if (!isdigit((unsigned char)arg[0]))
		return (-1);

	val = strtol(arg, &cp, 10);
	if (cp != NULL) {
		if (cp[0] != '\0' && cp[1] != '\0')
			 return (-1);
		switch (tolower((unsigned char)cp[0])) {
		case '\0':
		case 'b':
			break;
		case 'k':
			val <<= 10;
			break;
		case 'm':
			val <<= 20;
			break;
		case 'g':
			val <<= 30;
			break;
		default:
			return (-1);
		}
	}
	if (val < 0 || val > INT_MAX)
		return (-1);

	return (val);
}

/*
 * Set up socket buffer sizes before a connection is made.
 */
void
setupsockbufsize(int sock)
{

	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *) &sndbuf_size,
	    sizeof(rcvbuf_size)) < 0)
		warn("unable to set sndbuf size %d", sndbuf_size);

	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void *) &rcvbuf_size,
	    sizeof(rcvbuf_size)) < 0)
		warn("unable to set rcvbuf size %d", rcvbuf_size);
}

/*
 * Copy characters from src into dst, \ quoting characters that require it
 */
void
ftpvis(char *dst, size_t dstlen, const char *src, size_t srclen)
{
	int	di, si;

	for (di = si = 0;
	    src[si] != '\0' && di < dstlen && si < srclen;
	    di++, si++) {
		switch (src[si]) {
		case '\\':
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '"':
			dst[di++] = '\\';
			if (di >= dstlen)
				break;
			/* FALLTHROUGH */
		default:
			dst[di] = src[si];
		}
	}
	dst[di] = '\0';
}

/*
 * Copy src into buf (which is len bytes long), expanding % sequences.
 */
void
formatbuf(char *buf, size_t len, const char *src)
{
	const char	*p;
	char		*p2, *q;
	int		 i, op, updirs, pdirs;

#define ADDBUF(x) do { \
		if (i >= len - 1) \
			goto endbuf; \
		buf[i++] = (x); \
	} while (0)

	p = src;
	for (i = 0; *p; p++) {
		if (*p != '%') {
			ADDBUF(*p);
			continue;
		}
		p++;

		switch (op = *p) {

		case '/':
		case '.':
		case 'c':
			p2 = connected ? remotepwd : "";
			updirs = pdirs = 0;

			/* option to determine fixed # of dirs from path */
			if (op == '.' || op == 'c') {
				int skip;

				q = p2;
				while (*p2)		/* calc # of /'s */
					if (*p2++ == '/')
						updirs++;
				if (p[1] == '0') {	/* print <x> or ... */
					pdirs = 1;
					p++;
				}
				if (p[1] >= '1' && p[1] <= '9') {
							/* calc # to skip  */
					skip = p[1] - '0';
					p++;
				} else
					skip = 1;

				updirs -= skip;
				while (skip-- > 0) {
					while ((p2 > q) && (*p2 != '/'))
						p2--;	/* back up */
					if (skip && p2 > q)
						p2--;
				}
				if (*p2 == '/' && p2 != q)
					p2++;
			}

			if (updirs > 0 && pdirs) {
				if (i >= len - 5)
					break;
				if (op == '.') {
					ADDBUF('.');
					ADDBUF('.');
					ADDBUF('.');
				} else {
					ADDBUF('/');
					ADDBUF('<');
					if (updirs > 9) {
						ADDBUF('9');
						ADDBUF('+');
					} else
						ADDBUF('0' + updirs);
					ADDBUF('>');
				}
			}
			for (; *p2; p2++)
				ADDBUF(*p2);
			break;

		case 'M':
		case 'm':
			for (p2 = connected ? hostname : "-"; *p2; p2++) {
				if (op == 'm' && *p2 == '.')
					break;
				ADDBUF(*p2);
			}
			break;

		case 'n':
			for (p2 = connected ? username : "-"; *p2 ; p2++)
				ADDBUF(*p2);
			break;

		case '%':
			ADDBUF('%');
			break;

		default:		/* display unknown codes literally */
			ADDBUF('%');
			ADDBUF(op);
			break;

		}
	}
 endbuf:
	buf[i] = '\0';
}

/*
 * Determine if given string is an IPv6 address or not.
 * Return 1 for yes, 0 for no
 */
int
isipv6addr(const char *addr)
{
	int rv = 0;
#ifdef INET6
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(addr, "0", &hints, &res) != 0)
		rv = 0;
	else {
		rv = 1;
		freeaddrinfo(res);
	}
	if (debug)
		fprintf(ttyout, "isipv6addr: got %d for %s\n", rv, addr);
#endif
	return (rv == 1) ? 1 : 0;
}


/*
 * Internal version of connect(2); sets socket buffer sizes first.
 */
int
xconnect(int sock, const struct sockaddr *name, int namelen)
{

	setupsockbufsize(sock);
	return (connect(sock, name, namelen));
}

/*
 * Internal version of listen(2); sets socket buffer sizes first.
 */
int
xlisten(int sock, int backlog)
{

	setupsockbufsize(sock);
	return (listen(sock, backlog));
}

/*
 * malloc() with inbuilt error checking
 */
void *
xmalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (p == NULL)
		err(1, "Unable to allocate %ld bytes of memory", (long)size);
	return (p);
}

/*
 * sl_init() with inbuilt error checking
 */
StringList *
xsl_init(void)
{
	StringList *p;

	p = sl_init();
	if (p == NULL)
		err(1, "Unable to allocate memory for stringlist");
	return (p);
}

/*
 * sl_add() with inbuilt error checking
 */
void
xsl_add(StringList *sl, char *i)
{

	if (sl_add(sl, i) == -1)
		err(1, "Unable to add `%s' to stringlist", i);
}

/*
 * strdup() with inbuilt error checking
 */
char *
xstrdup(const char *str)
{
	char *s;

	if (str == NULL)
		errx(1, "xstrdup() called with NULL argument");
	s = strdup(str);
	if (s == NULL)
		err(1, "Unable to allocate memory for string copy");
	return (s);
}

/*
 * Install a POSIX signal handler, allowing the invoker to set whether
 * the signal should be restartable or not
 */
sigfunc
xsignal_restart(int sig, sigfunc func, int restartable)
{
	struct sigaction act, oact;
	act.sa_handler = func;

	sigemptyset(&act.sa_mask);
#if defined(SA_RESTART)			/* 4.4BSD, Posix(?), SVR4 */
	act.sa_flags = restartable ? SA_RESTART : 0;
#elif defined(SA_INTERRUPT)		/* SunOS 4.x */
	act.sa_flags = restartable ? 0 : SA_INTERRUPT;
#else
#error "system must have SA_RESTART or SA_INTERRUPT"
#endif
	if (sigaction(sig, &act, &oact) < 0)
		return (SIG_ERR);
	return (oact.sa_handler);
}

/*
 * Install a signal handler with the `restartable' flag set dependent upon
 * which signal is being set. (This is a wrapper to xsignal_restart())
 */
sigfunc
xsignal(int sig, sigfunc func)
{
	int restartable;

	/*
	 * Some signals print output or change the state of the process.
	 * There should be restartable, so that reads and writes are
	 * not affected.  Some signals should cause program flow to change;
	 * these signals should not be restartable, so that the system call
	 * will return with EINTR, and the program will go do something
	 * different.  If the signal handler calls longjmp() or siglongjmp(),
	 * it doesn't matter if it's restartable.
	 */

	switch(sig) {
#ifdef SIGINFO
	case SIGINFO:
#endif
	case SIGQUIT:
	case SIGUSR1:
	case SIGUSR2:
	case SIGWINCH:
		restartable = 1;
		break;

	case SIGALRM:
	case SIGINT:
	case SIGPIPE:
		restartable = 0;
		break;

	default:
		/*
		 * This is unpleasant, but I don't know what would be better.
		 * Right now, this "can't happen"
		 */
		errx(1, "xsignal_restart called with signal %d", sig);
	}

	return(xsignal_restart(sig, func, restartable));
}

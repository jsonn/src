/*	$NetBSD: ftp.c,v 1.64 1999/09/22 07:18:34 lukem Exp $	*/

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
#if 0
static char sccsid[] = "@(#)ftp.c	8.6 (Berkeley) 10/27/94";
#else
__RCSID("$NetBSD: ftp.c,v 1.64 1999/09/22 07:18:34 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#ifndef __USE_SELECT
#include <poll.h>
#endif

#include "ftp_var.h"

extern int h_errno;

int	data = -1;
int	abrtflag = 0;
jmp_buf	ptabort;
int	ptabflg;
int	ptflag = 0;
off_t	restart_point = 0;

static int empty __P((FILE *, FILE *, int));

union sockunion {
	struct sockinet {
#ifdef BSD4_4
		u_char si_len;
		u_char si_family;
#else
		u_short si_family;
#endif
		u_short si_port;
#ifndef BSD4_4
		u_char  si_pad[
#ifdef INET6
				sizeof(struct sockaddr_in6)
#else
				sizeof(struct sockaddr_in)
#endif
				- sizeof(u_int)];
		u_char	si_len;
#endif
	} su_si;
	struct sockaddr_in  su_sin;
#ifdef INET6
	struct sockaddr_in6 su_sin6;
#endif
};

#define su_len		su_si.si_len
#define su_family	su_si.si_family
#define su_port		su_si.si_port

union sockunion myctladdr, hisctladdr, data_addr;

FILE	*cin, *cout;

char *
hookup(host, port)
	char *host;
	char *port;
{
	int s = -1, len, error;
#ifdef NI_NUMERICHOST
	struct addrinfo hints, *res, *res0;
	char hbuf[MAXHOSTNAMELEN];
#else
	struct hostent *hp = NULL;
	struct servent *sp = NULL;
	char **ptr;
	struct sockaddr_in sin;
#endif
	static char hostnamebuf[MAXHOSTNAMELEN];
	char *cause = "unknown";
	int family;
	
#ifdef NI_NUMERICHOST
	memset((char *)&hisctladdr, 0, sizeof (hisctladdr));
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	error = getaddrinfo(host, port, &hints, &res0);
	if (error) {
		warnx(gai_strerror(error));
		code = -1;
		return (0);
	}

	if (res0->ai_canonname)
		strncpy(hostnamebuf, res0->ai_canonname, sizeof(hostnamebuf));
	else
		strncpy(hostnamebuf, host, sizeof(hostnamebuf));
	hostnamebuf[sizeof(hostnamebuf) - 1] = '\0';
	hostname = hostnamebuf;
	
	for (res = res0; res; res = res->ai_next) {
#if 0	/*old behavior*/
		if (res != res0)	/* not on the first address */
#else
		if (res0->ai_next)	/* if we have multiple possibilities */
#endif
		{
			getnameinfo(res->ai_addr, res->ai_addrlen,
				hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
			fprintf(ttyout, "Trying %s...\n", hbuf);
		}
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0) {
			cause = "socket";
			continue;
		}
		while ((error = xconnect(s, res->ai_addr, res->ai_addrlen)) < 0
				&& errno == EINTR) {
			;
		}
		if (error) {
			/* this "if" clause is to prevent print warning twice */
			if (res->ai_next) {
				getnameinfo(res->ai_addr, res->ai_addrlen,
					hbuf, sizeof(hbuf), NULL, 0,
					NI_NUMERICHOST);
				warn("connect to address %s", hbuf);
			}
			cause = "connect";
			close(s);
			s = -1;
			continue;
		}

		/* finally we got one */
		break;
	}
	if (s < 0) {
		warn(cause);
		code = -1;
		freeaddrinfo(res0);
		return 0;
	}
	memcpy(&hisctladdr, res->ai_addr, res->ai_addrlen);
	len = res->ai_addrlen;
	freeaddrinfo(res0);
	res0 = res = NULL;
	family = hisctladdr.su_family;
#else
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if ((hp = gethostbyname(host)) == NULL) {
		warnx("%s: %s", host, hstrerror(h_errno));
		code = -1;
		return 0;
	}

	if ((sp = getservbyname(port, "tcp")) == NULL) {
		sin.sin_port = htons(21);
	}
	else
		sin.sin_port = sp->s_port;

	strncpy(hostnamebuf, hp->h_name, sizeof(hostnamebuf));
	hostnamebuf[sizeof(hostnamebuf) - 1] = '\0';
	hostname = hostnamebuf;

	if (hp->h_length > sizeof(sin.sin_addr))
		hp->h_length = sizeof(sin.sin_addr);

	for (ptr = hp->h_addr_list; *ptr; ptr++) {
		memcpy(&sin.sin_addr, *ptr, (size_t)hp->h_length);
		if (hp->h_addr_list[1])
			fprintf(ttyout, "Trying %s...\n",
			    inet_ntoa(sin.sin_addr));
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0) {
			cause = "socket";
			continue;
		}
		while ((error = xconnect(s, (struct sockaddr *)&sin,
		    sizeof(sin))) < 0 && errno == EINTR) {
			;
		}
		if (error) {
			/* this "if" clause is to prevent print warning twice */
			if (hp->h_addr_list[1]) {
				warn("connect to address %s",
				    inet_ntoa(sin.sin_addr));
			}
			cause = "connect";
			close(s);
			s = -1;
			continue;
		}

		/* finally we got one */
		break;
	}
	if (s < 0) {
		warn(cause);
		code = -1;
		return 0;
	}
	memcpy(&hisctladdr, &sin, sizeof(sin));
	len = sizeof(sin);
	if (hisctladdr.su_len == 0)
		hisctladdr.su_len = len;
	family = AF_INET;
#endif

	if (getsockname(s, (struct sockaddr *)&myctladdr, &len) < 0) {
		warn("getsockname");
		code = -1;
		goto bad;
	}
	if (myctladdr.su_len == 0)
		myctladdr.su_len = len;

#if defined(IPPROTO_IP) && defined(IP_TOS)
	if (family == AF_INET) {
		int tos = IPTOS_LOWDELAY;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos,
			       sizeof(int)) < 0)
			warn("setsockopt TOS (ignored)");
	}
#endif
	cin = fdopen(s, "r");
	cout = fdopen(s, "w");
	if (cin == NULL || cout == NULL) {
		warnx("fdopen failed.");
		if (cin)
			(void)fclose(cin);
		if (cout)
			(void)fclose(cout);
		code = -1;
		goto bad;
	}
	if (verbose)
		fprintf(ttyout, "Connected to %s.\n", hostname);
	if (getreply(0) > 2) { 	/* read startup message from server */
		if (cin)
			(void)fclose(cin);
		if (cout)
			(void)fclose(cout);
		code = -1;
		goto bad;
	}
#ifdef SO_OOBINLINE
	{
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on))
		< 0 && debug) {
			warn("setsockopt");
		}
	}
#endif /* SO_OOBINLINE */

	return (hostname);
bad:
	(void)close(s);
	return (NULL);
}

void
cmdabort(notused)
	int notused;
{

	alarmtimer(0);
	putc('\n', ttyout);
	abrtflag++;
	if (ptflag)
		longjmp(ptabort, 1);
}

/*VARARGS*/
int
#ifdef __STDC__
command(const char *fmt, ...)
#else
command(va_alist)
	va_dcl
#endif
{
	va_list ap;
	int r;
	sig_t oldintr;
#ifndef __STDC__
	const char *fmt;
#endif

	abrtflag = 0;
	if (debug) {
		fputs("---> ", ttyout);
#ifdef __STDC__
		va_start(ap, fmt);
#else
		va_start(ap);
		fmt = va_arg(ap, const char *);
#endif
		if (strncmp("PASS ", fmt, 5) == 0)
			fputs("PASS XXXX", ttyout);
		else if (strncmp("ACCT ", fmt, 5) == 0)
			fputs("ACCT XXXX", ttyout);
		else
			vfprintf(ttyout, fmt, ap);
		va_end(ap);
		putc('\n', ttyout);
	}
	if (cout == NULL) {
		warnx("No control connection for command.");
		code = -1;
		return (0);
	}
	oldintr = signal(SIGINT, cmdabort);
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
	fmt = va_arg(ap, char *);
#endif
	vfprintf(cout, fmt, ap);
	va_end(ap);
	fputs("\r\n", cout);
	(void)fflush(cout);
	cpend = 1;
	r = getreply(!strcmp(fmt, "QUIT"));
	if (abrtflag && oldintr != SIG_IGN)
		(*oldintr)(SIGINT);
	(void)signal(SIGINT, oldintr);
	return (r);
}

char reply_string[BUFSIZ];		/* first line of previous reply */

int
getreply(expecteof)
	int expecteof;
{
	char current_line[BUFSIZ];	/* last line of previous reply */
	int c, n, line;
	int dig;
	int originalcode = 0, continuation = 0;
	sig_t oldintr;
	int pflag = 0;
	char *cp, *pt = pasv;

	oldintr = signal(SIGINT, cmdabort);
	for (line = 0 ;; line++) {
		dig = n = code = 0;
		cp = current_line;
		while ((c = getc(cin)) != '\n') {
			if (c == IAC) {     /* handle telnet commands */
				switch (c = getc(cin)) {
				case WILL:
				case WONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, DONT, c);
					(void)fflush(cout);
					break;
				case DO:
				case DONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, WONT, c);
					(void)fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void)signal(SIGINT, oldintr);
					code = 221;
					return (0);
				}
				lostpeer();
				if (verbose) {
					fputs(
	    "421 Service not available, remote server has closed connection.\n",
					    ttyout);
				}
				code = 421;
				return (4);
			}
			if (c != '\r' && (verbose > 0 ||
			    ((verbose > -1 && n == '5' && dig > 4) &&
			    (((!n && c < '5') || (n && n < '5'))
			     || !retry_connect)))) {
				if (proxflag &&
				   (dig == 1 || (dig == 5 && verbose == 0)))
					fprintf(ttyout, "%s:", hostname);
				(void)putc(c, ttyout);
			}
			if (dig < 4 && isdigit(c))
				code = code * 10 + (c - '0');
			if (!pflag && (code == 227 || code == 228))
				pflag = 1;
			else if (!pflag && code == 229)
				pflag = 100;
			if (dig > 4 && pflag == 1 && isdigit(c))
				pflag = 2;
			if (pflag == 2) {
				if (c != '\r' && c != ')')
					*pt++ = c;
				else {
					*pt = '\0';
					pflag = 3;
				}
			}
			if (pflag == 100 && c == '(')
				pflag = 2;
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
			if (cp < &current_line[sizeof(current_line) - 1])
				*cp++ = c;
		}
		if (verbose > 0 || ((verbose > -1 && n == '5') &&
		    (n < '5' || !retry_connect))) {
			(void)putc(c, ttyout);
			(void)fflush (ttyout);
		}
		if (line == 0) {
			size_t len = cp - current_line;

			if (len > sizeof(reply_string))
				len = sizeof(reply_string);

			(void)strncpy(reply_string, current_line, len);
			reply_string[len] = '\0';
		}
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		*cp = '\0';
		if (n != '1')
			cpend = 0;
		(void)signal(SIGINT, oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer();
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
			(*oldintr)(SIGINT);
		return (n - '0');
	}
}

static int
empty(cin, din, sec)
	FILE *cin;
	FILE *din;
	int sec;
{
	int nr;
	int nfd = 0;

#ifdef __USE_SELECT
	struct timeval t;
	fd_set rmask;

	FD_ZERO(&cin);
	if (cin) {
		if (nfd < fileno(cin))
			nfd = fileno(cin);
		FD_SET(fileno(cin), &rmask);
	}
	if (din) {
		if (nfd < fileno(din))
			nfd = fileno(din);
		FD_SET(fileno(din), &rmask);
	}
		
	t.tv_sec = (long) sec;
	t.tv_usec = 0;
	if ((nr = select(nfd, &rmask, NULL, NULL, &t)) <= 0)
		return nr;

	nr = 0;
	if (cin)
		nr |= FD_ISSET(fileno(cin), &rmask) ? 1 : 0;
	if (din)
		nr |= FD_ISSET(fileno(din), &rmask) ? 2 : 0;

#else
	struct pollfd pfd[2];

	if (cin) {
	    pfd[nfd].fd = fileno(cin);
	    pfd[nfd++].events = POLLIN;
	}

	if (din) {
	    pfd[nfd].fd = fileno(din);
	    pfd[nfd++].events = POLLIN;
	}

	if ((nr = poll(pfd, nfd, sec * 1000)) <= 0)
		return nr;

	nr = 0;
	nfd = 0;
	if (cin)
		nr |= (pfd[nfd++].revents & POLLIN) ? 1 : 0;
	if (din)
		nr |= (pfd[nfd++].revents & POLLIN) ? 2 : 0;
#endif
	return nr;
}

jmp_buf	sendabort;

void
abortsend(notused)
	int notused;
{

	alarmtimer(0);
	mflag = 0;
	abrtflag = 0;
	fputs("\nsend aborted\nwaiting for remote to finish abort.\n", ttyout);
	longjmp(sendabort, 1);
}

void
sendrequest(cmd, local, remote, printnames)
	const char *cmd, *local, *remote;
	int printnames;
{
	struct stat st;
	int c, d;
	FILE *fin, *dout;
	int (*closefunc) __P((FILE *));
	sig_t oldinti, oldintr, oldintp;
	volatile off_t hashbytes;
	char *lmode, *bufp;
	static size_t bufsize;
	static char *buf;
	int oprogress;

#ifdef __GNUC__			/* to shut up gcc warnings */
	(void)&fin;
	(void)&dout;
	(void)&closefunc;
	(void)&oldinti;
	(void)&oldintr;
	(void)&oldintp;
	(void)&lmode;
#endif

	hashbytes = mark;
	direction = "sent";
	dout = NULL;
	bytes = 0;
	filesize = -1;
	oprogress = progress;
	if (verbose && printnames) {
		if (local && *local != '-')
			fprintf(ttyout, "local: %s ", local);
		if (remote)
			fprintf(ttyout, "remote: %s\n", remote);
	}
	if (proxy) {
		proxtrans(cmd, local, remote);
		return;
	}
	if (curtype != type)
		changetype(type, 0);
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	oldinti = NULL;
	lmode = "w";
	if (setjmp(sendabort)) {
		while (cpend) {
			(void)getreply(0);
		}
		if (data >= 0) {
			(void)close(data);
			data = -1;
		}
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		if (oldintp)
			(void)signal(SIGPIPE, oldintp);
		if (oldinti)
			(void)xsignal(SIGINFO, oldinti);
		code = -1;
		goto cleanupsend;
	}
	oldintr = signal(SIGINT, abortsend);
	oldinti = xsignal(SIGINFO, psummary);
	if (strcmp(local, "-") == 0) {
		fin = stdin;
		progress = 0;
	} else if (*local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fin = popen(local + 1, "r");
		if (fin == NULL) {
			warn("%s", local + 1);
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGPIPE, oldintp);
			(void)xsignal(SIGINFO, oldinti);
			code = -1;
			goto cleanupsend;
		}
		progress = 0;
		closefunc = pclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			warn("local: %s", local);
			(void)signal(SIGINT, oldintr);
			(void)xsignal(SIGINFO, oldinti);
			code = -1;
			goto cleanupsend;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 || !S_ISREG(st.st_mode)) {
			fprintf(ttyout, "%s: not a plain file.\n", local);
			(void)signal(SIGINT, oldintr);
			(void)xsignal(SIGINFO, oldinti);
			fclose(fin);
			code = -1;
			goto cleanupsend;
		}
		filesize = st.st_size;
	}
	if (initconn()) {
		(void)signal(SIGINT, oldintr);
		(void)xsignal(SIGINFO, oldinti);
		if (oldintp)
			(void)signal(SIGPIPE, oldintp);
		code = -1;
		if (closefunc != NULL)
			(*closefunc)(fin);
		goto cleanupsend;
	}
	if (setjmp(sendabort))
		goto abort;

	if (restart_point &&
	    (strcmp(cmd, "STOR") == 0 || strcmp(cmd, "APPE") == 0)) {
		int rc;

		rc = -1;
		switch (curtype) {
		case TYPE_A:
			rc = fseek(fin, (long) restart_point, SEEK_SET);
			break;
		case TYPE_I:
		case TYPE_L:
			rc = lseek(fileno(fin), restart_point, SEEK_SET);
			break;
		}
		if (rc < 0) {
			warn("local: %s", local);
			if (closefunc != NULL)
				(*closefunc)(fin);
			goto cleanupsend;
		}
#ifndef NO_QUAD
		if (command("REST %qd", (long long) restart_point) !=
#else
		if (command("REST %ld", (long) restart_point) !=
#endif
		    CONTINUE) {
			if (closefunc != NULL)
				(*closefunc)(fin);
			goto cleanupsend;
		}
		lmode = "r+w";
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)xsignal(SIGINFO, oldinti);
			if (oldintp)
				(void)signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			goto cleanupsend;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)xsignal(SIGINFO, oldinti);
			if (oldintp)
				(void)signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			goto cleanupsend;
		}
	dout = dataconn(lmode);
	if (dout == NULL)
		goto abort;

	if (sndbuf_size > bufsize) {
		if (buf)
			(void)free(buf);
		bufsize = sndbuf_size;
		buf = xmalloc(bufsize);
	}
	if (debug)
		fprintf(ttyout, "using a buffer size of %d\n", (int)bufsize);

	progressmeter(-1);
	oldintp = signal(SIGPIPE, SIG_IGN);

	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		while (1) {
			struct timeval then, now, td;
			off_t bufrem;

			if (rate_put)
				(void)gettimeofday(&then, NULL);
			errno = c = d = 0;
			bufrem = rate_put ? rate_put : bufsize;
			while (bufrem > 0) {
				if ((c = read(fileno(fin), buf,
				    MIN(bufsize, bufrem))) <= 0)
					goto senddone;
				bytes += c;
				bufrem -= c;
				for (bufp = buf; c > 0; c -= d, bufp += d)
					if ((d = write(fileno(dout), bufp, c))
					    <= 0)
						break;
				if (d < 0)
					goto senddone;
				if (hash && (!progress || filesize < 0) ) {
					while (bytes >= hashbytes) {
						(void)putc('#', ttyout);
						hashbytes += mark;
					}
					(void)fflush(ttyout);
				}
			}
			if (rate_put) {
				while (1) {
					(void)gettimeofday(&now, NULL);
					timersub(&now, &then, &td);
					if (td.tv_sec > 0)
						break;
					usleep(1000000 - td.tv_usec);
				}
			}
		}
 senddone:
		if (hash && (!progress || filesize < 0) && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
		}
		if (c < 0)
			warn("local: %s", local);
		if (d < 0) {
			if (errno != EPIPE)
				warn("netout");
			bytes = -1;
		}
		break;

	case TYPE_A:
		while ((c = getc(fin)) != EOF) {
			if (c == '\n') {
				while (hash && (!progress || filesize < 0) &&
				    (bytes >= hashbytes)) {
					(void)putc('#', ttyout);
					(void)fflush(ttyout);
					hashbytes += mark;
				}
				if (ferror(dout))
					break;
				(void)putc('\r', dout);
				bytes++;
			}
			(void)putc(c, dout);
			bytes++;
#if 0	/* this violates RFC */
			if (c == '\r') {
				(void)putc('\0', dout);
				bytes++;
			}
#endif
		}
		if (hash && (!progress || filesize < 0)) {
			if (bytes < hashbytes)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
		}
		if (ferror(fin))
			warn("local: %s", local);
		if (ferror(dout)) {
			if (errno != EPIPE)
				warn("netout");
			bytes = -1;
		}
		break;
	}
	progressmeter(1);
	if (closefunc != NULL)
		(*closefunc)(fin);
	(void)fclose(dout);
	(void)getreply(0);
	(void)signal(SIGINT, oldintr);
	(void)xsignal(SIGINFO, oldinti);
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	if (bytes > 0)
		ptransfer(0);
	goto cleanupsend;
abort:
	(void)signal(SIGINT, oldintr);
	(void)xsignal(SIGINFO, oldinti);
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return;
	}
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (dout)
		(void)fclose(dout);
	(void)getreply(0);
	code = -1;
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
	if (bytes > 0)
		ptransfer(0);
cleanupsend:
	progress = oprogress;
	restart_point = 0;
}

jmp_buf	recvabort;

void
abortrecv(notused)
	int notused;
{

	alarmtimer(0);
	mflag = 0;
	abrtflag = 0;
	fputs("\nreceive aborted\nwaiting for remote to finish abort.\n",
	    ttyout);
	longjmp(recvabort, 1);
}

void
recvrequest(cmd, local, remote, lmode, printnames, ignorespecial)
	const char *cmd, *local, *remote, *lmode;
	int printnames, ignorespecial;
{
	FILE *fout, *din;
	int (*closefunc) __P((FILE *));
	sig_t oldinti, oldintr, oldintp;
	int c, d;
	volatile int is_retr, tcrflag, bare_lfs;
	static size_t bufsize;
	static char *buf;
	volatile off_t hashbytes;
	struct stat st;
	time_t mtime;
	struct timeval tval[2];
	int oprogress;
	int opreserve;

#ifdef __GNUC__			/* to shut up gcc warnings */
	(void)&local;
	(void)&fout;
	(void)&din;
	(void)&closefunc;
	(void)&oldinti;
	(void)&oldintr;
	(void)&oldintp;
#endif

	fout = NULL;
	din = NULL;
	oldinti = NULL;
	hashbytes = mark;
	direction = "received";
	bytes = 0;
	bare_lfs = 0;
	filesize = -1;
	oprogress = progress;
	opreserve = preserve;
	is_retr = (strcmp(cmd, "RETR") == 0);
	if (is_retr && verbose && printnames) {
		if (local && (ignorespecial || *local != '-'))
			fprintf(ttyout, "local: %s ", local);
		if (remote)
			fprintf(ttyout, "remote: %s\n", remote);
	}
	if (proxy && is_retr) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && is_retr;
	if (setjmp(recvabort)) {
		while (cpend) {
			(void)getreply(0);
		}
		if (data >= 0) {
			(void)close(data);
			data = -1;
		}
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		if (oldinti)
			(void)xsignal(SIGINFO, oldinti);
		progress = oprogress;
		preserve = opreserve;
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortrecv);
	oldinti = xsignal(SIGINFO, psummary);
	if (ignorespecial || (strcmp(local, "-") && *local != '|')) {
		if (access(local, W_OK) < 0) {
			char *dir = strrchr(local, '/');

			if (errno != ENOENT && errno != EACCES) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)xsignal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			if (dir != NULL)
				*dir = 0;
			d = access(dir == local ? "/" :
			    dir ? local : ".", W_OK);
			if (dir != NULL)
				*dir = '/';
			if (d < 0) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)xsignal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			if (!runique && errno == EACCES &&
			    chmod(local, (S_IRUSR|S_IWUSR)) < 0) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)xsignal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			if (runique && errno == EACCES &&
			   (local = gunique(local)) == NULL) {
				(void)signal(SIGINT, oldintr);
				(void)xsignal(SIGINFO, oldinti);
				code = -1;
				return;
			}
		}
		else if (runique && (local = gunique(local)) == NULL) {
			(void)signal(SIGINT, oldintr);
			(void)xsignal(SIGINFO, oldinti);
			code = -1;
			return;
		}
	}
	if (!is_retr) {
		if (curtype != TYPE_A)
			changetype(TYPE_A, 0);
	} else {
		if (curtype != type)
			changetype(type, 0);
		filesize = remotesize(remote, 0);
	}
	if (initconn()) {
		(void)signal(SIGINT, oldintr);
		(void)xsignal(SIGINFO, oldinti);
		code = -1;
		return;
	}
	if (setjmp(recvabort))
		goto abort;
	if (is_retr && restart_point &&
#ifndef NO_QUAD
	    command("REST %qd", (long long) restart_point) != CONTINUE)
#else
	    command("REST %ld", (long) restart_point) != CONTINUE)
#endif
		return;
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)xsignal(SIGINFO, oldinti);
			return;
		}
	} else {
		if (command("%s", cmd) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)xsignal(SIGINFO, oldinti);
			return;
		}
	}
	din = dataconn("r");
	if (din == NULL)
		goto abort;
	if (!ignorespecial && strcmp(local, "-") == 0) {
		fout = stdout;
		progress = 0;
		preserve = 0;
	} else if (!ignorespecial && *local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fout = popen(local + 1, "w");
		if (fout == NULL) {
			warn("%s", local+1);
			goto abort;
		}
		progress = 0;
		preserve = 0;
		closefunc = pclose;
	} else {
		fout = fopen(local, lmode);
		if (fout == NULL) {
			warn("local: %s", local);
			goto abort;
		}
		closefunc = fclose;
	}

	if (fstat(fileno(fout), &st) != -1 && !S_ISREG(st.st_mode)) {
		progress = 0;
		preserve = 0;
	}
	if (rcvbuf_size > bufsize) {
		if (buf)
			(void)free(buf);
		bufsize = rcvbuf_size;
		buf = xmalloc(bufsize);
	}
	if (debug)
		fprintf(ttyout, "using a buffer size of %d\n", (int)bufsize);

	progressmeter(-1);

	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		if (is_retr && restart_point &&
		    lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			warn("local: %s", local);
			progress = oprogress;
			preserve = opreserve;
			if (closefunc != NULL)
				(*closefunc)(fout);
			return;
		}
		while (1) {
			struct timeval then, now, td;
			off_t bufrem;

			if (rate_get)
				(void)gettimeofday(&then, NULL);
			errno = c = d = 0;
			bufrem = rate_get ? rate_get : bufsize;
			while (bufrem > 0) {
				if ((c = read(fileno(din), buf,
				    MIN(bufsize, bufrem))) <= 0)
					goto recvdone;
				bytes += c;
				bufrem -=c;
				if ((d = write(fileno(fout), buf, c)) != c)
					goto recvdone;
				if (hash && (!progress || filesize < 0)) {
					while (bytes >= hashbytes) {
						(void)putc('#', ttyout);
						hashbytes += mark;
					}
					(void)fflush(ttyout);
				}
			}
			if (rate_get) {
				while (1) {
					(void)gettimeofday(&now, NULL);
					timersub(&now, &then, &td);
					if (td.tv_sec > 0)
						break;
					usleep(1000000 - td.tv_usec);
				}
			}
		}
 recvdone:
		if (hash && (!progress || filesize < 0) && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
		}
		if (c < 0) {
			if (errno != EPIPE)
				warn("netin");
			bytes = -1;
		}
		if (d < c) {
			if (d < 0)
				warn("local: %s", local);
			else
				warnx("%s: short write", local);
		}
		break;

	case TYPE_A:
		if (is_retr && restart_point) {
			int ch;
			long i, n;

			if (fseek(fout, 0L, SEEK_SET) < 0)
				goto done;
			n = (long)restart_point;
			for (i = 0; i++ < n;) {
				if ((ch = getc(fout)) == EOF)
					goto done;
				if (ch == '\n')
					i++;
			}
			if (fseek(fout, 0L, SEEK_CUR) < 0) {
done:
				warn("local: %s", local);
				progress = oprogress;
				preserve = opreserve;
				if (closefunc != NULL)
					(*closefunc)(fout);
				return;
			}
		}
		while ((c = getc(din)) != EOF) {
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				while (hash && (!progress || filesize < 0) &&
				    (bytes >= hashbytes)) {
					(void)putc('#', ttyout);
					(void)fflush(ttyout);
					hashbytes += mark;
				}
				bytes++;
				if ((c = getc(din)) != '\n' || tcrflag) {
					if (ferror(fout))
						goto break2;
					(void)putc('\r', fout);
					if (c == '\0') {
						bytes++;
						goto contin2;
					}
					if (c == EOF)
						goto contin2;
				}
			}
			(void)putc(c, fout);
			bytes++;
	contin2:	;
		}
break2:
		if (hash && (!progress || filesize < 0)) {
			if (bytes < hashbytes)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
		}
		if (ferror(din)) {
			if (errno != EPIPE)
				warn("netin");
			bytes = -1;
		}
		if (ferror(fout))
			warn("local: %s", local);
		break;
	}
	progressmeter(1);
	if (closefunc != NULL)
		(*closefunc)(fout);
	(void)signal(SIGINT, oldintr);
	(void)xsignal(SIGINFO, oldinti);
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	(void)fclose(din);
	(void)getreply(0);
	if (bare_lfs) {
		fprintf(ttyout,
		    "WARNING! %d bare linefeeds received in ASCII mode.\n",
		    bare_lfs);
		fputs("File may not have transferred correctly.\n", ttyout);
	}
	if (bytes >= 0 && is_retr) {
		if (bytes > 0)
			ptransfer(0);
		if (preserve && (closefunc == fclose)) {
			mtime = remotemodtime(remote, 0);
			if (mtime != -1) {
				(void)gettimeofday(&tval[0], NULL);
				tval[1].tv_sec = mtime;
				tval[1].tv_usec = 0;
				if (utimes(local, tval) == -1) {
					fprintf(ttyout,
				"Can't change modification time on %s to %s",
					    local, asctime(localtime(&mtime)));
				}
			}
		}
	}
	progress = oprogress;
	preserve = opreserve;
	return;

abort:

/* abort using RFC 959 recommended IP,SYNC sequence */

	progress = oprogress;
	preserve = opreserve;
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	(void)signal(SIGINT, SIG_IGN);
	if (!cpend) {
		code = -1;
		(void)signal(SIGINT, oldintr);
		(void)xsignal(SIGINFO, oldinti);
		return;
	}

	abort_remote(din);
	code = -1;
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din)
		(void)fclose(din);
	if (bytes > 0)
		ptransfer(0);
	(void)signal(SIGINT, oldintr);
	(void)xsignal(SIGINFO, oldinti);
}

/*
 * Need to start a listen on the data channel before we send the command,
 * otherwise the server's connect may fail.
 */
int
initconn()
{
	char *p, *a;
	int result, len, tmpno = 0;
	int on = 1;
	int error;
	u_int addr[16], port[2];
	u_int af, hal, pal;
	char *pasvcmd = NULL;

#ifdef INET6
	if (myctladdr.su_family == AF_INET6
	 && (IN6_IS_ADDR_LINKLOCAL(&myctladdr.su_sin6.sin6_addr)
	  || IN6_IS_ADDR_SITELOCAL(&myctladdr.su_sin6.sin6_addr))) {
		warnx("use of scoped address can be troublesome");
	}
#endif
reinit:
	if (passivemode) {
		data_addr = myctladdr;
		data = socket(data_addr.su_family, SOCK_STREAM, 0);
		if (data < 0) {
			warn("socket");
			return (1);
		}
		if ((options & SO_DEBUG) &&
		    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			       sizeof(on)) < 0)
			warn("setsockopt (ignored)");
		result = COMPLETE + 1;
		switch (data_addr.su_family) {
		case AF_INET:
			if (epsv4) {
				result = command(pasvcmd = "EPSV");
				/*
				 * this code is to be friendly with broken
				 * BSDI ftpd
				 */
				if (code / 10 == 22 && code != 229) {
					fputs(
"wrong server: return code must be 229\n",
						ttyout);
					result = COMPLETE + 1;
				}
			}
			if (result != COMPLETE)
				result = command(pasvcmd = "PASV");
			break;
#ifdef INET6
		case AF_INET6:
			result = command(pasvcmd = "EPSV");
			/* this code is to be friendly with broken BSDI ftpd */
			if (code / 10 == 22 && code != 229) {
				fputs(
"wrong server: return code must be 229\n",
					ttyout);
				result = COMPLETE + 1;
			}
			if (result != COMPLETE)
				result = command(pasvcmd = "LPSV");
			break;
#endif
		default:
			result = COMPLETE + 1;
			break;
		}
		if (result != COMPLETE) {
			if (activefallback) {
				(void)close(data);
				data = -1;
				passivemode = 0;
				activefallback = 0;
				goto reinit;
			}
			fputs("Passive mode refused.\n", ttyout);
			goto bad;
		}

#define pack2(var, off) \
	(((var[(off) + 0] & 0xff) << 8) | ((var[(off) + 1] & 0xff) << 0))
#define pack4(var, off) \
	(((var[(off) + 0] & 0xff) << 24) | ((var[(off) + 1] & 0xff) << 16) | \
	 ((var[(off) + 2] & 0xff) << 8) | ((var[(off) + 3] & 0xff) << 0))

		/*
		 * What we've got at this point is a string of comma separated
		 * one-byte unsigned integer values, separated by commas.
		 */
		if (strcmp(pasvcmd, "PASV") == 0) {
			if (data_addr.su_family != AF_INET) {
				fputs(
"Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
				error = 1;
				goto bad;
			}
			if (code / 10 == 22 && code != 227) {
				fputs("wrong server: return code must be 227\n",
					ttyout);
				error = 1;
				goto bad;
			}
			error = sscanf(pasv, "%u,%u,%u,%u,%u,%u",
					&addr[0], &addr[1], &addr[2], &addr[3],
					&port[0], &port[1]);
			if (error != 6) {
				fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
				error = 1;
				goto bad;
			}
			error = 0;
			memset(&data_addr, 0, sizeof(data_addr));
			data_addr.su_family = AF_INET;
			data_addr.su_len = sizeof(struct sockaddr_in);
			data_addr.su_sin.sin_addr.s_addr =
				htonl(pack4(addr, 0));
			data_addr.su_port = htons(pack2(port, 0));
		} else if (strcmp(pasvcmd, "LPSV") == 0) {
			if (code / 10 == 22 && code != 228) {
				fputs("wrong server: return code must be 228\n",
					ttyout);
				error = 1;
				goto bad;
			}
			switch (data_addr.su_family) {
			case AF_INET:
				error = sscanf(pasv,
"%u,%u,%u,%u,%u,%u,%u,%u,%u",
					&af, &hal,
					&addr[0], &addr[1], &addr[2], &addr[3],
					&pal, &port[0], &port[1]);
				if (error != 9) {
					fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}
				if (af != 4 || hal != 4 || pal != 2) {
					fputs(
"Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}

				error = 0;
				memset(&data_addr, 0, sizeof(data_addr));
				data_addr.su_family = AF_INET;
				data_addr.su_len = sizeof(struct sockaddr_in);
				data_addr.su_sin.sin_addr.s_addr =
					htonl(pack4(addr, 0));
				data_addr.su_port = htons(pack2(port, 0));
				break;
#ifdef INET6
			case AF_INET6:
				error = sscanf(pasv,
"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
					&af, &hal,
					&addr[0], &addr[1], &addr[2], &addr[3],
					&addr[4], &addr[5], &addr[6], &addr[7],
					&addr[8], &addr[9], &addr[10],
					&addr[11], &addr[12], &addr[13],
					&addr[14], &addr[15],
					&pal, &port[0], &port[1]);
				if (error != 21) {
					fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}
				if (af != 6 || hal != 16 || pal != 2) {
					fputs(
"Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}

				error = 0;
				memset(&data_addr, 0, sizeof(data_addr));
				data_addr.su_family = AF_INET6;
				data_addr.su_len = sizeof(struct sockaddr_in6);
			    {
				u_int32_t *p32;
				p32 = (u_int32_t *)&data_addr.su_sin6.sin6_addr;
				p32[0] = htonl(pack4(addr, 0));
				p32[1] = htonl(pack4(addr, 4));
				p32[2] = htonl(pack4(addr, 8));
				p32[3] = htonl(pack4(addr, 12));
			    }
				data_addr.su_port = htons(pack2(port, 0));
				break;
#endif
			default:
				error = 1;
			}
		} else if (strcmp(pasvcmd, "EPSV") == 0) {
			char delim[4];

			port[0] = 0;
			if (code / 10 == 22 && code != 229) {
				fputs("wrong server: return code must be 229\n",
					ttyout);
				error = 1;
				goto bad;
			}
			if (sscanf(pasv, "%c%c%c%d%c", &delim[0],
					&delim[1], &delim[2], &port[1],
					&delim[3]) != 5) {
				fputs("parse error!\n", ttyout);
				error = 1;
				goto bad;
			}
			if (delim[0] != delim[1] || delim[0] != delim[2]
			 || delim[0] != delim[3]) {
				fputs("parse error!\n", ttyout);
				error = 1;
				goto bad;
			}
			data_addr = hisctladdr;
			data_addr.su_port = htons(port[1]);
		} else
			goto bad;

		while (xconnect(data, (struct sockaddr *)&data_addr,
			    data_addr.su_len) < 0) {
			if (errno == EINTR)
				continue;
			if (activefallback) {
				(void)close(data);
				data = -1;
				passivemode = 0;
				activefallback = 0;
				goto reinit;
			}
			warn("connect");
			goto bad;
		}
#if defined(IPPROTO_IP) && defined(IP_TOS)
		if (data_addr.su_family == AF_INET) {
			on = IPTOS_THROUGHPUT;
			if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on,
				       sizeof(int)) < 0)
				warn("setsockopt TOS (ignored)");
		}
#endif
		return (0);
	}

noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.su_port = 0;	/* let system pick one */
	if (data != -1)
		(void)close(data);
	data = socket(data_addr.su_family, SOCK_STREAM, 0);
	if (data < 0) {
		warn("socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
				sizeof(on)) < 0) {
			warn("setsockopt (reuse address)");
			goto bad;
		}
	if (bind(data, (struct sockaddr *)&data_addr, data_addr.su_len) < 0) {
		warn("bind");
		goto bad;
	}
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			sizeof(on)) < 0)
		warn("setsockopt (ignored)");
	len = sizeof(data_addr);
	if (getsockname(data, (struct sockaddr *)&data_addr, &len) < 0) {
		warn("getsockname");
		goto bad;
	}
	if (xlisten(data, 1) < 0)
		warn("listen");

#define	UC(b)	(((int)b)&0xff)

	if (sendport) {
#ifdef INET6
		char hname[INET6_ADDRSTRLEN];
		int af;
#endif

		switch (data_addr.su_family) {
		case AF_INET:
			if (!epsv4) {
				result = COMPLETE + 1;
				break;
			}
			/* FALLTHROUGH */
#ifdef INET6
		case AF_INET6:
			af = (data_addr.su_family == AF_INET) ? 1 : 2;
			if (getnameinfo((struct sockaddr *)&data_addr,
					data_addr.su_len, hname, sizeof(hname),
					NULL, 0, NI_NUMERICHOST)) {
				result = ERROR;
			} else {
				result = command("EPRT |%d|%s|%d|",
					af, hname, ntohs(data_addr.su_port));
			}
			break;
#endif
		default:
			result = COMPLETE + 1;
			break;
		}
		if (result == COMPLETE)
			goto skip_port;

		switch (data_addr.su_family) {
		case AF_INET:
			a = (char *)&data_addr.su_sin.sin_addr;
			p = (char *)&data_addr.su_port;
			result = command("PORT %d,%d,%d,%d,%d,%d",
				 UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				 UC(p[0]), UC(p[1]));
			break;
#ifdef INET6
		case AF_INET6:
			a = (char *)&data_addr.su_sin6.sin6_addr;
			p = (char *)&data_addr.su_port;
			result = command(
"LPRT %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
				 6, 16,
				 UC(a[0]),UC(a[1]),UC(a[2]),UC(a[3]),
				 UC(a[4]),UC(a[5]),UC(a[6]),UC(a[7]),
				 UC(a[8]),UC(a[9]),UC(a[10]),UC(a[11]),
				 UC(a[12]),UC(a[13]),UC(a[14]),UC(a[15]),
				 2, UC(p[0]), UC(p[1]));
			break;
#endif
		default:
			result = COMPLETE + 1; /* xxx */
		}
	skip_port:
		
		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
#if defined(IPPROTO_IP) && defined(IP_TOS)
	if (data_addr.su_family == AF_INET) {
		on = IPTOS_THROUGHPUT;
		if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on,
			       sizeof(int)) < 0)
			warn("setsockopt TOS (ignored)");
	}
#endif
	return (0);
bad:
	(void)close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
}

FILE *
dataconn(lmode)
	const char *lmode;
{
	union sockunion from;
	int s, fromlen = myctladdr.su_len;

	if (passivemode)
		return (fdopen(data, lmode));

	s = accept(data, (struct sockaddr *) &from, &fromlen);
	if (s < 0) {
		warn("accept");
		(void)close(data), data = -1;
		return (NULL);
	}
	(void)close(data);
	data = s;
#if defined(IPPROTO_IP) && defined(IP_TOS)
	if (from.su_family == AF_INET) {
		int tos = IPTOS_THROUGHPUT;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos,
				sizeof(int)) < 0) {
			warn("setsockopt TOS (ignored)");
		}
	}
#endif
	return (fdopen(data, lmode));
}

void
psummary(notused)
	int notused;
{
	int oerrno;

	oerrno = errno;
	if (bytes > 0)
		ptransfer(1);
	errno = oerrno;
}

void
psabort(notused)
	int notused;
{

	alarmtimer(0);
	abrtflag++;
}

void
pswitch(flag)
	int flag;
{
	sig_t oldintr;
	static struct comvars {
		int connect;
		char name[MAXHOSTNAMELEN];
		union sockunion mctl;
		union sockunion hctl;
		FILE *in;
		FILE *out;
		int tpe;
		int curtpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[MAXPATHLEN];
		char mo[MAXPATHLEN];
	} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = signal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname) {
		(void)strncpy(ip->name, hostname, sizeof(ip->name) - 1);
		ip->name[sizeof(ip->name) - 1] = '\0';
	} else
		ip->name[0] = '\0';
	hostname = op->name;
	ip->hctl = hisctladdr;
	hisctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = cin;
	cin = op->in;
	ip->out = cout;
	cout = op->out;
	ip->tpe = type;
	type = op->tpe;
	ip->curtpe = curtype;
	curtype = op->curtpe;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void)strncpy(ip->nti, ntin, sizeof(ip->nti) - 1);
	(ip->nti)[sizeof(ip->nti) - 1] = '\0';
	(void)strcpy(ntin, op->nti);
	(void)strncpy(ip->nto, ntout, sizeof(ip->nto) - 1);
	(ip->nto)[sizeof(ip->nto) - 1] = '\0';
	(void)strcpy(ntout, op->nto);
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void)strncpy(ip->mi, mapin, sizeof(ip->mi) - 1);
	(ip->mi)[sizeof(ip->mi) - 1] = '\0';
	(void)strcpy(mapin, op->mi);
	(void)strncpy(ip->mo, mapout, sizeof(ip->mo) - 1);
	(ip->mo)[sizeof(ip->mo) - 1] = '\0';
	(void)strcpy(mapout, op->mo);
	(void)signal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)(SIGINT);
	}
}

void
abortpt(notused)
	int notused;
{

	alarmtimer(0);
	putc('\n', ttyout);
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	longjmp(ptabort, 1);
}

void
proxtrans(cmd, local, remote)
	const char *cmd, *local, *remote;
{
	sig_t oldintr;
	int prox_type, nfnd;
	volatile int secndflag;
	char *cmd2;

#ifdef __GNUC__			/* to shut up gcc warnings */
	(void)&oldintr;
	(void)&cmd2;
#endif

	oldintr = NULL;
	secndflag = 0;
	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if ((prox_type = type) == 0) {
		if (unix_server && unix_proxy)
			prox_type = TYPE_I;
		else
			prox_type = TYPE_A;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PASV") != COMPLETE) {
		fputs("proxy server does not support third party transfers.\n",
		    ttyout);
		return;
	}
	pswitch(0);
	if (!connected) {
		fputs("No primary connection.\n", ttyout);
		pswitch(1);
		code = -1;
		return;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PORT %s", pasv) != COMPLETE) {
		pswitch(1);
		return;
	}
	if (setjmp(ptabort))
		goto abort;
	oldintr = signal(SIGINT, abortpt);
	if ((restart_point &&
#ifndef NO_QUAD
	    (command("REST %qd", (long long) restart_point) != CONTINUE)
#else
	    (command("REST %ld", (long) restart_point) != CONTINUE)
#endif
	    ) || (command("%s %s", cmd, remote) != PRELIM)) {
		(void)signal(SIGINT, oldintr);
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if ((restart_point &&
#ifndef NO_QUAD
	    (command("REST %qd", (long long) restart_point) != CONTINUE)
#else
	    (command("REST %ld", (long) restart_point) != CONTINUE)
#endif
	    ) || (command("%s %s", cmd2, local) != PRELIM))
		goto abort;
	ptflag++;
	(void)getreply(0);
	pswitch(0);
	(void)getreply(0);
	(void)signal(SIGINT, oldintr);
	pswitch(1);
	ptflag = 0;
	fprintf(ttyout, "local: %s remote: %s\n", local, remote);
	return;
abort:
	(void)signal(SIGINT, SIG_IGN);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if (!strcmp(cmd, "RETR") && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote(NULL);
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void)signal(SIGINT, oldintr);
		return;
	}
	if (cpend)
		abort_remote(NULL);
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote(NULL);
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void)signal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend)
		abort_remote(NULL);
	pswitch(!proxy);
	if (cpend) {
		if ((nfnd = empty(cin, NULL, 10)) <= 0) {
			if (nfnd < 0) {
				warn("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer();
		}
		(void)getreply(0);
		(void)getreply(0);
	}
	if (proxy)
		pswitch(0);
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void)signal(SIGINT, oldintr);
}

void
reset(argc, argv)
	int argc;
	char *argv[];
{
	int nfnd = 1;

	while (nfnd > 0) {
		if ((nfnd = empty(cin, NULL, 0)) < 0) {
			warn("reset");
			code = -1;
			lostpeer();
		}
		else if (nfnd) {
			(void)getreply(0);
		}
	}
}

char *
gunique(local)
	const char *local;
{
	static char new[MAXPATHLEN];
	char *cp = strrchr(local, '/');
	int d, count=0;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp == local ? "/" : cp ? local : ".", W_OK);
	if (cp)
		*cp = '/';
	if (d < 0) {
		warn("local: %s", local);
		return (NULL);
	}
	(void)strcpy(new, local);
	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			fputs("runique: can't find unique file name.\n",
			    ttyout);
			return (NULL);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, F_OK)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return (new);
}

void
abort_remote(din)
	FILE *din;
{
	char buf[MIN(4, BUFSIZ)];
	int nfnd;

	if (cout == NULL) {
		warnx("Lost control connection for abort.");
		if (ptabflg)
			code = -1;
		lostpeer();
		return;
	}
	/*
	 * send IAC in urgent mode instead of DM because 4.3BSD places oob mark
	 * after urgent byte rather than before as is protocol now
	 */
	buf[0] = IAC;
	buf[1] = IP;
	buf[2] = IAC;
	if (send(fileno(cout), buf, 3, MSG_OOB) != 3)
		warn("abort");
	fprintf(cout, "%cABOR\r\n", DM);
	(void)fflush(cout);
	if ((nfnd = empty(cin, din, 10)) <= 0) {
		if (nfnd < 0) {
			warn("abort");
		}
		if (ptabflg)
			code = -1;
		lostpeer();
	}
	if (din && (nfnd & 2)) {
		while (read(fileno(din), buf, BUFSIZ) > 0)
			continue;
	}
	if (getreply(0) == ERROR && code == 552) {
		/* 552 needed for nic style abort */
		(void)getreply(0);
	}
	(void)getreply(0);
}

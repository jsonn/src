/*	$NetBSD: tftp.c,v 1.11.4.4 2004/04/09 04:23:35 jmc Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
static char sccsid[] = "@(#)tftp.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: tftp.c,v 1.11.4.4 2004/04/09 04:23:35 jmc Exp $");
#endif
#endif /* not lint */

/* Many bug fixes are from Jim Guyton <guyton@rand-unix> */

/*
 * TFTP User Program -- Protocol Machines
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/tftp.h>

#include <err.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "extern.h"
#include "tftpsubs.h"

extern  struct sockaddr_storage peeraddr; /* filled in by main */
extern  int     f;			/* the opened socket */
extern  int     trace;
extern  int     verbose;
extern  int     def_rexmtval;
extern  int     rexmtval;
extern  int     maxtimeout;
extern	int	tsize;
extern	int	tout;
extern	int	def_blksize;
extern	int	blksize;

char    ackbuf[PKTSIZE];
int	timeout;
jmp_buf	toplevel;
jmp_buf	timeoutbuf;

static void nak __P((int, struct sockaddr *));
static int makerequest __P((int, const char *, struct tftphdr *, const char *, off_t));
static void printstats __P((const char *, unsigned long));
static void startclock __P((void));
static void stopclock __P((void));
static void timer __P((int));
static void tpacket __P((const char *, struct tftphdr *, int));
static int cmpport __P((struct sockaddr *, struct sockaddr *));

static void get_options(struct tftphdr *, int);

static void
get_options(struct tftphdr *ap, int size)
{
	unsigned long val;
	char *opt, *endp, *nextopt, *valp;
	int l;

	size -= 2;	/* skip over opcode */
	opt = ap->th_stuff;
	endp = opt + size - 1;
	*endp = '\0';
	
	while (opt < endp) {
		l = strlen(opt) + 1;
		valp = opt + l;
		if (valp < endp) {
			val = strtoul(valp, NULL, 10);
			l = strlen(valp) + 1;
			nextopt = valp + l;
			if (val == ULONG_MAX && errno == ERANGE) {
				/* Report illegal value */
				opt = nextopt;
				continue;
			}
		} else {
			/* Badly formed OACK */
			break;
		}
		if (strcmp(opt, "tsize") == 0) {
			/* cool, but we'll ignore it */
		} else if (strcmp(opt, "timeout") == 0) {
			if (val >= 1 && val <= 255) {
				rexmtval = val;
			} else {
				/* Report error? */
			}
		} else if (strcmp(opt, "blksize") == 0) {
			if (val >= 8 && val <= MAXSEGSIZE) {
				blksize = val;
			} else {
				/* Report error? */
			}
		} else {
			/* unknown option */
		}
		opt = nextopt;
	}
}

/*
 * Send the requested file.
 */
void
sendfile(fd, name, mode)
	int fd;
	char *name;
	char *mode;
{
	struct tftphdr *ap;	   /* data and ack packets */
	struct tftphdr *dp;
	int n;
	volatile unsigned int block;
	volatile int size, convert;
	volatile unsigned long amount;
	struct sockaddr_storage from;
	struct stat sbuf;
	off_t filesize=0;
	int fromlen;
	FILE *file;
	struct sockaddr_storage peer;
	struct sockaddr_storage serv;	/* valid server port number */

	startclock();		/* start stat's clock */
	dp = r_init();		/* reset fillbuf/read-ahead code */
	ap = (struct tftphdr *)ackbuf;
	if (tsize) {
		if (fstat(fd, &sbuf) == 0) {
			filesize = sbuf.st_size;
		} else {
			filesize = -1ULL;
		}
	}
	file = fdopen(fd, "r");
	convert = !strcmp(mode, "netascii");
	block = 0;
	amount = 0;
	memcpy(&peer, &peeraddr, peeraddr.ss_len);
	memset(&serv, 0, sizeof(serv));

	signal(SIGALRM, timer);
	do {
		if (block == 0)
			size = makerequest(WRQ, name, dp, mode, filesize) - 4;
		else {
		/*	size = read(fd, dp->th_data, SEGSIZE);	 */
			size = readit(file, &dp, blksize, convert);
			if (size < 0) {
				nak(errno + 100, (struct sockaddr *)&peer);
				break;
			}
			dp->th_opcode = htons((u_short)DATA);
			dp->th_block = htons((u_short)block);
		}
		timeout = 0;
		(void) setjmp(timeoutbuf);
send_data:
		if (trace)
			tpacket("sent", dp, size + 4);
		n = sendto(f, dp, size + 4, 0,
		    (struct sockaddr *)&peer, peer.ss_len);
		if (n != size + 4) {
			warn("sendto");
			goto abort;
		}
		if (block)
			read_ahead(file, blksize, convert);
		for ( ; ; ) {
			alarm(rexmtval);
			do {
				fromlen = sizeof(from);
				n = recvfrom(f, ackbuf, sizeof(ackbuf), 0,
				    (struct sockaddr *)&from, &fromlen);
			} while (n <= 0);
			alarm(0);
			if (n < 0) {
				warn("recvfrom");
				goto abort;
			}
			if (!serv.ss_family)
				serv = from;
			else if (!cmpport((struct sockaddr *)&serv,
			    (struct sockaddr *)&from)) {
				warn("server port mismatch");
				goto abort;
			}
			peer = from;
			if (trace)
				tpacket("received", ap, n);
			/* should verify packet came from server */
			ap->th_opcode = ntohs(ap->th_opcode);
			ap->th_block = ntohs(ap->th_block);
			if (ap->th_opcode == ERROR) {
				printf("Error code %d: %s\n", ap->th_code,
					ap->th_msg);
				goto abort;
			}
			if (ap->th_opcode == ACK) {
				int j;

				if (ap->th_block == 0) {
					/*
					 * If the extended options are enabled,
					 * the server just refused 'em all.
					 * The only one that _really_
					 * matters is blksize, but we'll
					 * clear timeout, too.
					 */
					blksize = def_blksize;
					rexmtval = def_rexmtval;
				}
				if (ap->th_block == block) {
					break;
				}
				/* On an error, try to synchronize
				 * both sides.
				 */
				j = synchnet(f, blksize+4);
				if (j && trace) {
					printf("discarded %d packets\n",
							j);
				}
				if (ap->th_block == (block-1)) {
					goto send_data;
				}
			}
			if (ap->th_opcode == OACK) {
				if (block == 0) {
					blksize = def_blksize;
					rexmtval = def_rexmtval;
					get_options(ap, n);
					break;
				}
			}
		}
		if (block > 0)
			amount += size;
		block++;
	} while (size == blksize || block == 1);
abort:
	fclose(file);
	stopclock();
	if (amount > 0)
		printstats("Sent", amount);
}

/*
 * Receive a file.
 */
void
recvfile(fd, name, mode)
	int fd;
	char *name;
	char *mode;
{
	struct tftphdr *ap;
	struct tftphdr *dp;
	int n, oack=0;
	volatile unsigned int block;
	volatile int size, firsttrip;
	volatile unsigned long amount;
	struct sockaddr_storage from;
	int fromlen, readlen;
	FILE *file;
	volatile int convert;		/* true if converting crlf -> lf */
	struct sockaddr_storage peer;
	struct sockaddr_storage serv;	/* valid server port number */

	startclock();
	dp = w_init();
	ap = (struct tftphdr *)ackbuf;
	file = fdopen(fd, "w");
	convert = !strcmp(mode, "netascii");
	block = 1;
	firsttrip = 1;
	amount = 0;
	memcpy(&peer, &peeraddr, peeraddr.ss_len);
	memset(&serv, 0, sizeof(serv));

	signal(SIGALRM, timer);
	do {
		if (firsttrip) {
			size = makerequest(RRQ, name, ap, mode, 0);
			readlen = PKTSIZE;
			firsttrip = 0;
		} else {
			ap->th_opcode = htons((u_short)ACK);
			ap->th_block = htons((u_short)(block));
			readlen = blksize+4;
			size = 4;
			block++;
		}
		timeout = 0;
		(void) setjmp(timeoutbuf);
send_ack:
		if (trace)
			tpacket("sent", ap, size);
		if (sendto(f, ackbuf, size, 0, (struct sockaddr *)&peer,
		    peer.ss_len) != size) {
			alarm(0);
			warn("sendto");
			goto abort;
		}
		write_behind(file, convert);
		for ( ; ; ) {
			alarm(rexmtval);
			do  {
				fromlen = sizeof(from);
				n = recvfrom(f, dp, readlen, 0,
				    (struct sockaddr *)&from, &fromlen);
			} while (n <= 0);
			alarm(0);
			if (n < 0) {
				warn("recvfrom");
				goto abort;
			}
			if (!serv.ss_family)
				serv = from;
			else if (!cmpport((struct sockaddr *)&serv,
			    (struct sockaddr *)&from)) {
				warn("server port mismatch");
				goto abort;
			}
			peer = from;
			if (trace)
				tpacket("received", dp, n);
			/* should verify client address */
			dp->th_opcode = ntohs(dp->th_opcode);
			dp->th_block = ntohs(dp->th_block);
			if (dp->th_opcode == ERROR) {
				printf("Error code %d: %s\n", dp->th_code,
					dp->th_msg);
				goto abort;
			}
			if (dp->th_opcode == DATA) {
				int j;

				if (dp->th_block == 1 && !oack) {
					/* no OACK, revert to defaults */
					blksize = def_blksize;
					rexmtval = def_rexmtval;
				}
				if (dp->th_block == block) {
					break;		/* have next packet */
				}
				/* On an error, try to synchronize
				 * both sides.
				 */
				j = synchnet(f, blksize);
				if (j && trace) {
					printf("discarded %d packets\n", j);
				}
				if (dp->th_block == (block-1)) {
					goto send_ack;	/* resend ack */
				}
			}
			if (dp->th_opcode == OACK) {
				if (block == 1) {
					oack = 1;
					blksize = def_blksize;
					rexmtval = def_rexmtval;
					get_options(dp, n);
					ap->th_opcode = htons(ACK);
					ap->th_block = 0;
					readlen = blksize+4;
					size = 4;
					goto send_ack;
				}
			}
		}
	/*	size = write(fd, dp->th_data, n - 4); */
		size = writeit(file, &dp, n - 4, convert);
		if (size < 0) {
			nak(errno + 100, (struct sockaddr *)&peer);
			break;
		}
		amount += size;
	} while (size == blksize || block == 1);
abort:						/* ok to ack, since user */
	ap->th_opcode = htons((u_short)ACK);	/* has seen err msg */
	ap->th_block = htons((u_short)block);
	(void) sendto(f, ackbuf, 4, 0, (struct sockaddr *)&peer,
	    peer.ss_len);
	write_behind(file, convert);		/* flush last buffer */
	fclose(file);
	stopclock();
	if (amount > 0)
		printstats("Received", amount);
}

static int
makerequest(request, name, tp, mode, filesize)
	int request;
	const char *name;
	struct tftphdr *tp;
	const char *mode;
	off_t filesize;
{
	char *cp;

	tp->th_opcode = htons((u_short)request);
#ifndef __SVR4
	cp = tp->th_stuff;
#else
	cp = (void *)&tp->th_stuff;
#endif
	strcpy(cp, name);
	cp += strlen(name);
	*cp++ = '\0';
	strcpy(cp, mode);
	cp += strlen(mode);
	*cp++ = '\0';
	if (tsize) {
		strcpy(cp, "tsize");
		cp += strlen(cp);
		*cp++ = '\0';
		sprintf(cp, "%lu", (unsigned long) filesize);
		cp += strlen(cp);
		*cp++ = '\0';
	}
	if (tout) {
		strcpy(cp, "timeout");
		cp += strlen(cp);
		*cp++ = '\0';
		sprintf(cp, "%d", rexmtval);
		cp += strlen(cp);
		*cp++ = '\0';
	}
	if (blksize != SEGSIZE) {
		strcpy(cp, "blksize");
		cp += strlen(cp);
		*cp++ = '\0';
		sprintf(cp, "%d", blksize);
		cp += strlen(cp);
		*cp++ = '\0';
	}
	return (cp - (char *)tp);
}

const struct errmsg {
	int	e_code;
	const char *e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal TFTP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ EOPTNEG,	"Option negotiation failed" },
	{ -1,		0 }
};

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
static void
nak(error, peer)
	int error;
	struct sockaddr *peer;
{
	const struct errmsg *pe;
	struct tftphdr *tp;
	int length;
	size_t msglen;

	tp = (struct tftphdr *)ackbuf;
	tp->th_opcode = htons((u_short)ERROR);
	msglen = sizeof(ackbuf) - (&tp->th_msg[0] - ackbuf);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		tp->th_code = EUNDEF;
		strlcpy(tp->th_msg, strerror(error - 100), msglen);
	} else {
		tp->th_code = htons((u_short)error);
		strlcpy(tp->th_msg, pe->e_msg, msglen);
	}
	length = strlen(tp->th_msg);
	msglen = &tp->th_msg[length + 1] - ackbuf;
	if (trace)
		tpacket("sent", tp, (int)msglen);
	if (sendto(f, ackbuf, msglen, 0, peer, peer->sa_len) != msglen)
		warn("nak");
}

static void
tpacket(s, tp, n)
	const char *s;
	struct tftphdr *tp;
	int n;
{
	static char *opcodes[] =
	   { "#0", "RRQ", "WRQ", "DATA", "ACK", "ERROR", "OACK" };
	char *cp, *file, *endp, *opt, *spc;
	u_short op = ntohs(tp->th_opcode);
	int i, o;

	if (op < RRQ || op > OACK)
		printf("%s opcode=%x ", s, op);
	else
		printf("%s %s ", s, opcodes[op]);
	switch (op) {

	case RRQ:
	case WRQ:
		n -= 2;
#ifndef __SVR4
		cp = tp->th_stuff;
#else
		cp = (void *) &tp->th_stuff;
#endif
		endp = cp + n - 1;
		if (*endp != '\0') {	/* Shouldn't happen, but... */
			*endp = '\0';
		}
		file = cp;
		cp = strchr(cp, '\0') + 1;
		printf("<file=%s, mode=%s", file, cp);
		cp = strchr(cp, '\0') + 1;
		o = 0;
		while (cp < endp) {
			i = strlen(cp) + 1;
			if (o) {
				printf(", %s=%s", opt, cp);
			} else {
				opt = cp;
			}
			o = (o+1) % 2;
			cp += i;
		}
		printf(">\n");
		break;

	case DATA:
		printf("<block=%d, %d bytes>\n", ntohs(tp->th_block), n - 4);
		break;

	case ACK:
		printf("<block=%d>\n", ntohs(tp->th_block));
		break;

	case ERROR:
		printf("<code=%d, msg=%s>\n", ntohs(tp->th_code), tp->th_msg);
		break;

	case OACK:
		o = 0;
		n -= 2;
		cp = tp->th_stuff;
		endp = cp + n - 1;
		if (*endp != '\0') {	/* Shouldn't happen, but... */
			*endp = '\0';
		}
		printf("<");
		spc = "";
		while (cp < endp) {
			i = strlen(cp) + 1;
			if (o) {
				printf("%s%s=%s", spc, opt, cp);
				spc = ", ";
			} else {
				opt = cp;
			}
			o = (o+1) % 2;
			cp += i;
		}
		printf(">\n");
		break;
	}
}

struct timeval tstart;
struct timeval tstop;

static void
startclock()
{

	(void)gettimeofday(&tstart, NULL);
}

static void
stopclock()
{

	(void)gettimeofday(&tstop, NULL);
}

static void
printstats(direction, amount)
	const char *direction;
	unsigned long amount;
{
	double delta;

	/* compute delta in 1/10's second units */
	delta = ((tstop.tv_sec*10.)+(tstop.tv_usec/100000)) -
		((tstart.tv_sec*10.)+(tstart.tv_usec/100000));
	delta = delta/10.;      /* back to seconds */
	printf("%s %ld bytes in %.1f seconds", direction, amount, delta);
	if (verbose)
		printf(" [%.0f bits/sec]", (amount*8.)/delta);
	putchar('\n');
}

static void
timer(sig)
	int sig;
{

	timeout += rexmtval;
	if (timeout >= maxtimeout) {
		printf("Transfer timed out.\n");
		longjmp(toplevel, -1);
	}
	longjmp(timeoutbuf, 1);
}

static int
cmpport(sa, sb)
	struct sockaddr *sa;
	struct sockaddr *sb;
{
	char a[NI_MAXSERV], b[NI_MAXSERV];

	if (getnameinfo(sa, sa->sa_len, NULL, 0, a, sizeof(a), NI_NUMERICSERV))
		return 0;
	if (getnameinfo(sb, sb->sa_len, NULL, 0, b, sizeof(b), NI_NUMERICSERV))
		return 0;
	if (strcmp(a, b) != 0)
		return 0;

	return 1;
}

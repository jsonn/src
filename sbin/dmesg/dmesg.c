/*	$NetBSD: dmesg.c,v 1.25.18.1 2008/09/28 11:17:10 mjf Exp $	*/
/*-
 * Copyright (c) 1991, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
__COPYRIGHT("@(#) Copyright (c) 1991, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)dmesg.c	8.1 (Berkeley) 6/5/93";
#else
__RCSID("$NetBSD: dmesg.c,v 1.25.18.1 2008/09/28 11:17:10 mjf Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/msgbuf.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <vis.h>

#ifndef SMALL
struct nlist nl[] = {
#define	X_MSGBUF	0
	{ .n_name = "_msgbufp" },
	{ .n_name = NULL },
};

void	usage(void);

#define	KREAD(addr, var) \
	kvm_read(kd, addr, &var, sizeof(var)) != sizeof(var)
#endif

int
main(int argc, char *argv[])
{
	struct kern_msgbuf cur;
	int ch, newl, skip, i;
	char *p, *bufdata;
#ifndef SMALL
	char *memf, *nlistf;
#endif
	char buf[5];

#ifndef SMALL
	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "M:N:")) != -1)
		switch(ch) {
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (memf == NULL) {
#endif
		size_t size;
		int mib[2];

		mib[0] = CTL_KERN;
		mib[1] = KERN_MSGBUF;

		if (sysctl(mib, 2, NULL, &size, NULL, 0) == -1 ||
		    (bufdata = malloc(size)) == NULL ||
		    sysctl(mib, 2, bufdata, &size, NULL, 0) == -1)
			err(1, "can't get msgbuf");

		/* make a dummy struct msgbuf for the display logic */
		cur.msg_bufx = 0;
		cur.msg_bufs = size;
#ifndef SMALL
	} else {
		kvm_t *kd;
		struct kern_msgbuf *bufp;

		/*
		 * Read in message buffer header and data, and do sanity
		 * checks.
		 */
		kd = kvm_open(nlistf, memf, NULL, O_RDONLY, "dmesg");
		if (kd == NULL)
			exit (1);
		if (kvm_nlist(kd, nl) == -1)
			errx(1, "kvm_nlist: %s", kvm_geterr(kd));
		if (nl[X_MSGBUF].n_type == 0)
			errx(1, "%s: msgbufp not found", nlistf ? nlistf :
			    "namelist");
		if (KREAD(nl[X_MSGBUF].n_value, bufp))
			errx(1, "kvm_read: %s (0x%lx)", kvm_geterr(kd),
			    nl[X_MSGBUF].n_value);
		if (kvm_read(kd, (long)bufp, &cur,
		    offsetof(struct kern_msgbuf, msg_bufc)) !=
		    offsetof(struct kern_msgbuf, msg_bufc))
			errx(1, "kvm_read: %s (0x%lx)", kvm_geterr(kd),
			    (unsigned long)bufp);
		if (cur.msg_magic != MSG_MAGIC)
			errx(1, "magic number incorrect");
		bufdata = malloc(cur.msg_bufs);
		if (bufdata == NULL)
			errx(1, "couldn't allocate space for buffer data");
		if (kvm_read(kd, (long)&bufp->msg_bufc, bufdata,
		    cur.msg_bufs) != cur.msg_bufs)
			errx(1, "kvm_read: %s", kvm_geterr(kd));
		kvm_close(kd);
		if (cur.msg_bufx >= cur.msg_bufs)
			cur.msg_bufx = 0;
	}
#endif

	/*
	 * The message buffer is circular; start at the write pointer
	 * (which points the oldest character), and go to the write
	 * pointer - 1 (which points the newest character).  I.e, loop
	 * over cur.msg_bufs times.  Unused area is skipped since it
	 * contains nul.
	 */
	for (newl = skip = i = 0, p = bufdata + cur.msg_bufx;
	    i < cur.msg_bufs; i++, p++) {
#ifndef SMALL
		if (p == bufdata + cur.msg_bufs)
			p = bufdata;
#endif
		ch = *p;
		/* Skip "\n<.*>" syslog sequences. */
		if (skip) {
			if (ch == '>')
				newl = skip = 0;
			continue;
		}
		if (newl && ch == '<') {
			skip = 1;
			continue;
		}
		if (ch == '\0')
			continue;
		newl = ch == '\n';
		(void)vis(buf, ch, VIS_NOSLASH, 0);
#ifndef SMALL
		if (buf[1] == 0)
			(void)putchar(buf[0]);
		else
#endif
			(void)printf("%s", buf);
	}
	if (!newl)
		(void)putchar('\n');
	exit(0);
}

#ifndef SMALL
void
usage(void)
{

	(void)fprintf(stderr, "usage: dmesg [-M core] [-N system]\n");
	exit(1);
}
#endif

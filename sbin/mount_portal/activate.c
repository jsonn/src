/*	$NetBSD: activate.c,v 1.14.22.1 2009/05/13 19:19:03 jym Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *
 *	from: Id: activate.c,v 1.2 1992/05/27 07:09:27 jsp Exp
 *	@(#)activate.c	8.3 (Berkeley) 4/28/95
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: activate.c,v 1.14.22.1 2009/05/13 19:19:03 jym Exp $");
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include "portald.h"

static	int	get_request(int, struct portal_cred *, char *, size_t);
static	void	send_reply(int, int, int);

/*
 * Scan the providers list and call the
 * appropriate function.
 */
int
activate_argv(struct portal_cred *pcr, char *key, char **v, int *fdp)
{
	provider *pr;

	for (pr = providers; pr->pr_match; pr++)
		if (strcmp(v[0], pr->pr_match) == 0)
			return ((*pr->pr_func)(pcr, key, v, fdp));

	return (ENOENT);
}

static int
get_request(int so, struct portal_cred *pcr, char *key, size_t klen)
{
	struct iovec iov[2];
	struct msghdr msg;
	ssize_t n;

	iov[0].iov_base = (caddr_t) pcr;
	iov[0].iov_len = sizeof(*pcr);
	iov[1].iov_base = key;
	iov[1].iov_len = klen;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;

	n = recvmsg(so, &msg, 0);
	if (n < 0)
		return (errno);

	if (n <= (ssize_t)sizeof(*pcr))
		return (EINVAL);

	n -= sizeof(*pcr);
	key[n] = '\0';

	return (0);
}

static void
send_reply(int so, int fd, int error)
{
	int n;
	struct iovec iov;
	struct msghdr msg;
	void *ctl = NULL;
	struct cmsghdr *cmsg;
	int *files;
	socklen_t cmsgsize;

	/*
	 * Line up error code.  Don't worry about byte ordering
	 * because we must be sending to the local machine.
	 */
	iov.iov_base = (caddr_t) &error;
	iov.iov_len = sizeof(error);

	/*
	 * Build a msghdr
	 */
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/*
	 * If there is a file descriptor to send then
	 * construct a suitable rights control message.
	 */
	if (fd >= 0) {
		cmsgsize = CMSG_LEN(sizeof(*files));

		ctl = malloc(cmsgsize);
		if (ctl == NULL) {
			syslog(LOG_WARNING, "malloc control message: %m");
			return;
		}
		memset(ctl, 0, cmsgsize);

		cmsg = (struct cmsghdr *) ctl;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;

		files = (int *)CMSG_DATA(cmsg);
		files[0] = fd;

		msg.msg_control = ctl;
		msg.msg_controllen = cmsgsize;
	}

	/*
	 * Send to kernel...
	 */
	if ((n = sendmsg(so, &msg, MSG_EOR)) < 0)
		syslog(LOG_WARNING, "send: %m");
#ifdef DEBUG
	fprintf(stderr, "sent %d bytes\n", n);
#endif
	sleep(1);	/*XXX*/
#ifdef notdef
	if (shutdown(so, 2) < 0)
		syslog(LOG_WARNING, "shutdown: %m");
#endif
	/*
	 * Throw away the open file descriptor and control
	 * message buffer.
	 */
	if (fd >= 0)
		(void) close(fd);
	if (ctl != NULL)
		free(ctl);
}

void
activate(qelem *q, int so)
{
	struct portal_cred pcred;
	char key[MAXPATHLEN+1];
	int error;
	char **v;
	int fd = -1;

	/*
	 * Read the key from the socket
	 */
	error = get_request(so, &pcred, key, sizeof(key));
	if (error) {
		syslog(LOG_WARNING, "activate: recvmsg: %m");
		goto drop;
	}

#ifdef DEBUG
	fprintf(stderr, "lookup key %s\n", key);
#endif

	/*
	 * Find a match in the configuration file
	 */
	v = conf_match(q, key);

	/*
	 * If a match existed, then find an appropriate portal
	 * otherwise simply return ENOENT.
	 */
	if (v) {
		error = activate_argv(&pcred, key, v, &fd);
		if (error)
			fd = -1;
		else if (fd < 0)
			error = -1;
	} else
		error = ENOENT;

	if (error >= 0)
		send_reply(so, fd, error);

drop:;
	close(so);
}

/*	$NetBSD: fifo.c,v 1.4.2.1 2008/05/18 12:30:48 yamt Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn and Jaromir Dolecek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/param.h>
#include <sys/event.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

int
main(int argc, char **argv)
{
	int kq, n, fd, error = 0;
	struct kevent event[1];
	char buffer[128], name[MAXPATHLEN];
	const char *dir = "/tmp";
	
	if (argc != 1 && argc != 2) {
		printf("fifo: incorrect number of arguments\n");
		printf("usage:  fifo [directory]\n");
		exit(1);
	}

	if (argc == 2)
		dir = argv[1];

	if (snprintf(name, sizeof(name), "%s/fifo.XXXXXX", dir) >= sizeof(name))
		errx(1, "dir '%s' too long", dir);

	if ((fd = mkstemp(name)) == -1)
		err(1, "mkstemp %s", name);
	if (close(fd) == -1)
		err(1, "close %d (%s)", fd, name);

	if (unlink(name) == -1)
		err(1, "unlink %s", name);
	    		/* We're not concerned about a potential race here */
	if (mkfifo(name, 0644) < 0)
		err(1, "mkfifo %s", name);

	printf("fifo: created fifo '%s'\n", name);

	if ((fd = open(name, O_RDWR, 0644)) < 0)
		err(1, "open %s", name);

        kq = kqueue();
        if (kq < 0) {
                warn("kqueue");
		error = 1;
		goto out;
	}

	event[0].ident = fd;
	event[0].filter = EVFILT_READ;
	event[0].flags = EV_ADD | EV_ENABLE;
	n = kevent(kq, event, 1, NULL, 0, NULL);
	if (n < 0) {
		warn("kevent(1)");
		error = 1;
		goto out;
	}
	
	/* make sure there is something in the fifo */
	(void) write(fd, "foo", 3);
	printf("fifo: wrote 'foo' to fifo\n");

	memset(event, 0, sizeof(event));

	n = kevent(kq, NULL, 0, event, 1, NULL);
	if (n < 0) {
		warn("kevent(2)");
		error = 1;
		goto out;
	}

	printf("kevent num %d filt %d flags: %#x, fflags: %#x, data: %" PRId64 "\n",
		n, event[0].filter,
		event[0].flags, event[0].fflags, event[0].data);
	if (event[0].filter != EVFILT_READ)
		printf("fifo: incorrect filter, expecting EVFILT_READ\n");

	n = read(fd, buffer, event[0].data);
	if (n < 0) {
		warn("read");
		error = 1;
		goto out;
	}
	buffer[n] = '\0';
	printf("fifo: read '%s'\n", buffer);


	printf("fifo: successful end\n");

  out:
	unlink(name);
	printf("fifo: unlinked fifo '%s'\n", name);

	exit(error);
}

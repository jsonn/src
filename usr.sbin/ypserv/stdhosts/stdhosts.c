/*	$NetBSD: stdhosts.c,v 1.5.2.1 1997/11/28 09:39:27 mellon Exp $	 */

/*
 * Copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: stdhosts.c,v 1.5.2.1 1997/11/28 09:39:27 mellon Exp $");
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "protos.h"

int	main __P((int, char *[]));
void	usage __P((void));

extern	char *__progname;	/* from crt0.o */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct in_addr host_addr;
	FILE	*data_file;
	int	 line_no;
	size_t	 len;
	char	*k, *v, *addr_string, *fname;

	addr_string = NULL;		/* XXX gcc -Wuninitialized */

	if (argc > 2)
		usage();

	if (argc == 2) {
		fname = argv[1];
		data_file = fopen(fname, "r");
		if (data_file == NULL)
			err(1, "%s", fname); 
	} else {
		fname = "<stdin>";
		data_file = stdin;
	}

	line_no = 0;
	while ((v = read_line(data_file, &len, &line_no)) != NULL) {
		if (len == 0 || *v == '#')
			continue;

		k = strchr(v, '#');
		if (k != NULL)
			*k = '\0';

		for (k = v; *v && !isspace(*v); v++)
			;
		while (*v && isspace(*v))
			*v++ = '\0';

		if (inet_aton(k, &host_addr) == 0 ||
		    (addr_string = inet_ntoa(host_addr)) == NULL)
			warnx("%s line %d: syntax error", fname, line_no);

		printf("%s %s\n", addr_string, v);
	}

	exit(0);
}

void
usage()
{

	fprintf(stderr, "usage: %s [file]\n", __progname);
	exit(1);
}

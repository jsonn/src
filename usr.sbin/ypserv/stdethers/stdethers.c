/*	$NetBSD: stdethers.c,v 1.6.2.1 1997/11/28 09:38:26 mellon Exp $	*/

/*
 * Copyright (c) 1995 Mats O Jansson <moj@stacken.kth.se>
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
__RCSID("$NetBSD: stdethers.c,v 1.6.2.1 1997/11/28 09:38:26 mellon Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <netinet/in.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "protos.h"

int	main __P((int, char *[]));
void	usage __P((void));

extern	char *__progname;		/* from crt0.o */


int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct ether_addr eth_addr;
	FILE	*data_file;
	int	 line_no;
	size_t	 len;
	char	*fname, *p, *h;
	char	 hostname[MAXHOSTNAMELEN + 1];

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
	while ((p = read_line(data_file, &len, &line_no)) != NULL) {
		if (len == 0 || *p == '#')
			continue;

		h = strchr(p, '#');
		if (h != NULL)
			*h = '\0';

		if (ether_line(p, &eth_addr, hostname) == 0)
			printf("%s\t%s\n", ether_ntoa(&eth_addr), hostname);
		else
			warnx("ignoring line %d: `%s'", line_no, p);
	}

	exit(0);
}

void
usage()
{

	fprintf(stderr, "usage: %s [file]\n", __progname);
	exit(1);
}

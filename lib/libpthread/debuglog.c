/*	$NetBSD: debuglog.c,v 1.1.2.3 2001/07/31 00:15:37 nathanw Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nathan J. Williams.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>

#include "pthread.h"
#include "pthread_int.h"


void pthread__debuglog_read(int);
void usage(void);


static struct pthread_msgbuf* debugbuf;

int
main(int argc, char *argv[])
{
	int ch;
	extern int optind;
	extern char *optarg;

	int follow, initialize;

	follow = 0;
	initialize = 0;

	while ((ch = getopt(argc, argv, "fi")) != -1)
		switch (ch) {           
		case 'f': 
			follow = 1;
			break;
		case 'i':
			initialize = 1;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	debugbuf = pthread__debuglog_init(initialize);

	pthread__debuglog_read(follow);
	
	return 0;
}

void
pthread__debuglog_read(int follow)
{

	int readp, writep;

	do {

		readp = debugbuf->msg_bufr;
		writep = debugbuf->msg_bufw;

		if (readp < writep) {
			printf("%.*s", writep - readp, 
			    &debugbuf->msg_bufc[readp]);
		} else if (readp > writep) {
			printf("%.*s", (int)debugbuf->msg_bufs - readp, 
			    &debugbuf->msg_bufc[readp]);
			printf("%.*s", writep, &debugbuf->msg_bufc[0]);
		}

		debugbuf->msg_bufr = writep;

		if (follow)
			sleep(1);
	} while (follow);
}




void usage()
{

  fprintf(stderr,"usage: debuglog [-fi]\n");
  exit(1);
}

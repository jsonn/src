/*	$NetBSD: btpin.c,v 1.3.12.1 2008/09/18 04:29:08 wrstuden Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2006 Itronix, Inc.  All rights reserved.");
__RCSID("$NetBSD: btpin.c,v 1.3.12.1 2008/09/18 04:29:08 wrstuden Exp $");

#include <sys/types.h>
#include <sys/un.h>
#include <bluetooth.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int  main(int, char *[]);
void usage(void);

int
main(int ac, char *av[])
{
	bthcid_pin_response_t rp;
	struct sockaddr_un un;
	char *pin = NULL;
	int ch, s, len;

	memset(&rp, 0, sizeof(rp));
	len = -1;

	memset(&un, 0, sizeof(un));
	un.sun_len = sizeof(un);
	un.sun_family = AF_LOCAL;
	strlcpy(un.sun_path, BTHCID_SOCKET_NAME, sizeof(un.sun_path));

	while ((ch = getopt(ac, av, "a:d:l:p:rs:")) != EOF) {
		switch (ch) {
		case 'a':
			if (!bt_aton(optarg, &rp.raddr)) {
				struct hostent  *he = NULL;

				if ((he = bt_gethostbyname(optarg)) == NULL)
					errx(EXIT_FAILURE, "%s: %s", optarg,
							hstrerror(h_errno));

				bdaddr_copy(&rp.raddr, (bdaddr_t *)he->h_addr);
			}
			break;

		case 'd':
			if (!bt_devaddr(optarg, &rp.laddr))
				err(EXIT_FAILURE, "%s", optarg);

			break;

		case 'l':
			len = atoi(optarg);
			if (len < 1 || len > HCI_PIN_SIZE)
				errx(EXIT_FAILURE, "Invalid PIN length");

			break;

		case 'p':
			pin = optarg;
			break;

		case 'r':
			if (len == -1)
				len = 4;

			break;

		case 's':
			strlcpy(un.sun_path, optarg, sizeof(un.sun_path));
			break;

		default:
			usage();
		}
	}

	if (bdaddr_any(&rp.raddr))
		usage();

	if (pin == NULL) {
		if (len == -1)
			usage();

		srandom(time(NULL));

		pin = (char *)rp.pin;
		while (len-- > 0)
			*pin++ = '0' + (random() % 10);

		printf("PIN: %.*s\n", HCI_PIN_SIZE, rp.pin);
	} else {
		if (len != -1)
			usage();

		strncpy((char *)rp.pin, pin, HCI_PIN_SIZE);
	}

	s = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (s < 0)
		err(EXIT_FAILURE, "socket");

	if (connect(s, (struct sockaddr *)&un, sizeof(un)) < 0)
		err(EXIT_FAILURE, "connect(\"%s\")", un.sun_path);
	
	if (send(s, &rp, sizeof(rp), 0) != sizeof(rp))
		err(EXIT_FAILURE, "send");

	close(s);
	exit(EXIT_SUCCESS);
}

void
usage(void)
{

	fprintf(stderr,
		"usage: %s [-d device] [-s socket] {-p pin | -r [-l len]} -a addr\n"
		"", getprogname());

	exit(EXIT_FAILURE);
}

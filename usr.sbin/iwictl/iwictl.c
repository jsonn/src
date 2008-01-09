/*	$NetBSD: iwictl.c,v 1.7.8.1 2008/01/09 02:02:05 matt Exp $	*/

/*-
 * Copyright (c) 2004, 2005
 *	Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: iwictl.c,v 1.7.8.1 2008/01/09 02:02:05 matt Exp $");

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <net/if.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define SIOCGRADIO	_IOWR('i', 139, struct ifreq)
#define SIOCGTABLE0	_IOWR('i', 140, struct ifreq)

static void usage(void) __dead;
static int do_req(const char *, unsigned long, void *);
static void get_radio_state(const char *);
static void get_statistics(const char *);

int
main(int argc, char **argv)
{
	int ch;
	char *iface = NULL;
	int noflag = 1, rflag = 0;

	setprogname(argv[0]);
	if (argc > 1 && argv[1][0] != '-') {
		iface = argv[1];
		optind++;
	}

	while ((ch = getopt(argc, argv, "i:r")) != -1) {
		if (ch != 'i')
			noflag = 0;

		switch (ch) {
		case 'i':
			iface = optarg;
			break;

		case 'r':
			rflag = 1;
			break;

		default:
			usage();
		}
	}

	if (iface == NULL)
		usage();

	if (rflag)
		get_radio_state(iface);

	if (noflag)
		get_statistics(iface);

	return EX_OK;
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage:  %s [-i] iface\n"
	    "\t%s [-i] iface -r\n", getprogname(), getprogname());

	exit(EX_USAGE);
}

static int
do_req(const char *iface, unsigned long req, void *data)
{
	int s;
	struct ifreq ifr;
	int error, serrno;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(EX_OSERR, "Can't create socket");

	(void)memset(&ifr, 0, sizeof(ifr));
	(void)strncpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = data;
	error = ioctl(s, req, &ifr);
	serrno = errno;
	(void)close(s);
	errno = serrno;

	return error;
}

static void
get_radio_state(const char *iface)
{
	int radio;

	if (do_req(iface, SIOCGRADIO, &radio) == -1)
		err(EX_OSERR, "Can't read radio");

	(void)printf("Radio is %s\n", radio ? "ON" : "OFF");
}

struct statistic {
	int 		index;
	const char	*desc;
};

static const struct statistic tbl[] = {
	{  1, "Current transmission rate" },
	{  2, "Fragmentation threshold" },
	{  3, "RTS threshold" },
	{  4, "Number of frames submitted for transfer" },
	{  5, "Number of frames transmitted" },
	{  6, "Number of unicast frames transmitted" },
	{  7, "Number of unicast 802.11b frames transmitted at 1Mb/s" },
	{  8, "Number of unicast 802.11b frames transmitted at 2Mb/s" },
	{  9, "Number of unicast 802.11b frames transmitted at 5.5Mb/s" },
	{ 10, "Number of unicast 802.11b frames transmitted at 11Mb/s" },

	{ 19, "Number of unicast 802.11g frames transmitted at 1Mb/s" },
	{ 20, "Number of unicast 802.11g frames transmitted at 2Mb/s" },
	{ 21, "Number of unicast 802.11g frames transmitted at 5.5Mb/s" },
	{ 22, "Number of unicast 802.11g frames transmitted at 6Mb/s" },
	{ 23, "Number of unicast 802.11g frames transmitted at 9Mb/s" },
	{ 24, "Number of unicast 802.11g frames transmitted at 11Mb/s" },
	{ 25, "Number of unicast 802.11g frames transmitted at 12Mb/s" },
	{ 26, "Number of unicast 802.11g frames transmitted at 18Mb/s" },
	{ 27, "Number of unicast 802.11g frames transmitted at 24Mb/s" },
	{ 28, "Number of unicast 802.11g frames transmitted at 36Mb/s" },
	{ 29, "Number of unicast 802.11g frames transmitted at 48Mb/s" },
	{ 30, "Number of unicast 802.11g frames transmitted at 54Mb/s" },
	{ 31, "Number of multicast frames transmitted" },
	{ 32, "Number of multicast 802.11b frames transmitted at 1Mb/s" },
	{ 33, "Number of multicast 802.11b frames transmitted at 2Mb/s" },
	{ 34, "Number of multicast 802.11b frames transmitted at 5.5Mb/s" },
	{ 35, "Number of multicast 802.11b frames transmitted at 11Mb/s" },

	{ 44, "Number of multicast 802.11g frames transmitted at 1Mb/s" },
	{ 45, "Number of multicast 802.11g frames transmitted at 2Mb/s" },
	{ 46, "Number of multicast 802.11g frames transmitted at 5.5Mb/s" },
	{ 47, "Number of multicast 802.11g frames transmitted at 6Mb/s" },
	{ 48, "Number of multicast 802.11g frames transmitted at 9Mb/s" },
	{ 49, "Number of multicast 802.11g frames transmitted at 11Mb/s" },
	{ 50, "Number of multicast 802.11g frames transmitted at 12Mb/s" },
	{ 51, "Number of multicast 802.11g frames transmitted at 18Mb/s" },
	{ 52, "Number of multicast 802.11g frames transmitted at 24Mb/s" },
	{ 53, "Number of multicast 802.11g frames transmitted at 36Mb/s" },
	{ 54, "Number of multicast 802.11g frames transmitted at 48Mb/s" },
	{ 55, "Number of multicast 802.11g frames transmitted at 54Mb/s" },
	{ 56, "Number of transmission retries" },
	{ 57, "Number of transmission failures" },
	{ 58, "Number of frames with a bad CRC received" },

	{ 61, "Number of full scans" },
	{ 62, "Number of partial scans" },

	{ 64, "Number of bytes transmitted" },
	{ 65, "Current RSSI" },
	{ 66, "Number of beacons received" },
	{ 67, "Number of beacons missed" },

	{ 0, NULL }
};

static void
get_statistics(const char *iface)
{
	static u_int32_t stats[256];
	const struct statistic *st;

	if (do_req(iface, SIOCGTABLE0, stats) == -1)
		err(EX_OSERR, "Can't read statistics");

	for (st = tbl; st->index != 0; st++)
		(void)printf("%-60s[%u]\n", st->desc, stats[st->index]);
}

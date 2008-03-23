/* gpioctl.c,v 1.1 2005/09/27 02:54:27 jmcneill Exp */
/*	$OpenBSD: gpioctl.c,v 1.2 2004/08/08 00:05:09 deraadt Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Program to control GPIO devices.
 */

#include <sys/types.h>
#include <sys/gpio.h>
#include <sys/ioctl.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _PATH_DEV_GPIO	"/dev/gpio0"

static const char *device = _PATH_DEV_GPIO;
static int devfd = -1;
static int quiet = 0;

static void	getinfo(void);
static void	pinread(int);
static void	pinwrite(int, int);
static void	pinctl(int, char *[], int);
static void 	usage(void);

static const struct bitstr {
	unsigned int mask;
	const char *string;
} pinflags[] = {
	{ GPIO_PIN_INPUT, "in" },
	{ GPIO_PIN_OUTPUT, "out" },
	{ GPIO_PIN_INOUT, "inout" },
	{ GPIO_PIN_OPENDRAIN, "od" },
	{ GPIO_PIN_PUSHPULL, "pp" },
	{ GPIO_PIN_TRISTATE, "tri" },
	{ GPIO_PIN_PULLUP, "pu" },
	{ GPIO_PIN_PULLDOWN, "pd" },
	{ GPIO_PIN_INVIN, "iin" },
	{ GPIO_PIN_INVOUT, "iiout" },
	{ 0, NULL },
};

int
main(int argc, char *argv[])
{
	int ch;
	char *ep;
	int do_ctl = 0;
	int pin = 0, value = 0;

	setprogname(argv[0]);

	while ((ch = getopt(argc, argv, "cd:hq")) != -1)
		switch (ch) {
		case 'c':
			do_ctl = 1;
			break;
		case 'd':
			device = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'h':
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		pin = strtol(argv[0], &ep, 10);
		if (*argv[0] == '\0' || *ep != '\0' || pin < 0)
			errx(EXIT_FAILURE, "%s: invalid pin", argv[0]);
	}

	if ((devfd = open(device, O_RDWR)) == -1)
		err(EXIT_FAILURE, "%s", device);

	if (argc == 0 && !do_ctl) {
		getinfo();
	} else if (argc == 1) {
		if (do_ctl)
			pinctl(pin, NULL, 0);
		else
			pinread(pin);
	} else if (argc > 1) {
		if (do_ctl) {
			pinctl(pin, argv + 1, argc - 1);
		} else {
			value = strtol(argv[1], &ep, 10);
			if (*argv[1] == '\0' || *ep != '\0')
				errx(EXIT_FAILURE, "%s: invalid value",
				    argv[1]);
			pinwrite(pin, value);
		}
	} else {
		usage();
		/* NOTREACHED */
	}

	return EXIT_SUCCESS;
}

static void
getinfo(void)
{
	struct gpio_info info;

	memset(&info, 0, sizeof(info));
	if (ioctl(devfd, GPIOINFO, &info) == -1)
		err(EXIT_FAILURE, "GPIOINFO");

	if (quiet)
		return;

	printf("%s: %d pins\n", device, info.gpio_npins);
}

static void
pinread(int pin)
{
	struct gpio_pin_op op;

	memset(&op, 0, sizeof(op));
	op.gp_pin = pin;
	if (ioctl(devfd, GPIOPINREAD, &op) == -1)
		err(EXIT_FAILURE, "GPIOPINREAD");

	if (quiet)
		return;

	printf("pin %d: state %d\n", pin, op.gp_value);
}

static void
pinwrite(int pin, int value)
{
	struct gpio_pin_op op;

	if (value < 0 || value > 2)
		errx(EXIT_FAILURE, "%d: invalid value", value);

	memset(&op, 0, sizeof(op));
	op.gp_pin = pin;
	op.gp_value = (value == 0 ? GPIO_PIN_LOW : GPIO_PIN_HIGH);
	if (value < 2) {
		if (ioctl(devfd, GPIOPINWRITE, &op) == -1)
			err(EXIT_FAILURE, "GPIOPINWRITE");
	} else {
		if (ioctl(devfd, GPIOPINTOGGLE, &op) == -1)
			err(EXIT_FAILURE, "GPIOPINTOGGLE");
	}

	if (quiet)
		return;

	printf("pin %d: state %d -> %d\n", pin, op.gp_value,
	    (value < 2 ? value : 1 - op.gp_value));
}

static void
pinctl(int pin, char *flags[], int nflags)
{
	struct gpio_pin_ctl ctl;
	int fl = 0;
	const struct bitstr *bs;
	int i;

	memset(&ctl, 0, sizeof(ctl));
	ctl.gp_pin = pin;
	if (flags != NULL) {
		for (i = 0; i < nflags; i++)
			for (bs = pinflags; bs->string != NULL; bs++)
				if (strcmp(flags[i], bs->string) == 0) {
					fl |= bs->mask;
					break;
				}
	}
	ctl.gp_flags = fl;
	if (ioctl(devfd, GPIOPINCTL, &ctl) == -1)
		err(EXIT_FAILURE, "GPIOPINCTL");

	if (quiet)
		return;

	printf("pin %d: caps:", pin);
	for (bs = pinflags; bs->string != NULL; bs++)
		if (ctl.gp_caps & bs->mask)
			printf(" %s", bs->string);
	printf(", flags:");
	for (bs = pinflags; bs->string != NULL; bs++)
		if (ctl.gp_flags & bs->mask)
			printf(" %s", bs->string);
	if (fl > 0) {
		printf(" ->");
		for (bs = pinflags; bs->string != NULL; bs++)
			if (fl & bs->mask)
				printf(" %s", bs->string);
	}
	printf("\n");
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-hq] [-d device] [pin] [0 | 1 | 2]\n",
	    getprogname());
	fprintf(stderr, "       %s [-hq] [-d device] -c pin [flags]\n",
	    getprogname());

	exit(EXIT_FAILURE);
}

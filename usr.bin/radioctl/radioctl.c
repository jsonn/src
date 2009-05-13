/* $NetBSD: radioctl.c,v 1.7.30.1 2009/05/13 19:20:02 jym Exp $ */
/* $OpenBSD: radioctl.c,v 1.5 2001/12/18 18:42:19 mickey Exp $ */
/* $RuOBSD: radioctl.c,v 1.4 2001/10/20 18:09:10 pva Exp $ */

/*
 * Copyright (c) 2001 Vladimir Popov <jumbo@narod.ru>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>

#ifndef lint
__RCSID("$NetBSD: radioctl.c,v 1.7.30.1 2009/05/13 19:20:02 jym Exp $");
#endif

#include <sys/ioctl.h>
#include <sys/radioio.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RADIO_ENV	"RADIODEVICE"
#define RADIODEVICE	"/dev/radio"

const char *varname[] = {
	"search",
#define OPTION_SEARCH		0x00
	"volume",
#define OPTION_VOLUME		0x01
	"frequency",
#define OPTION_FREQUENCY	0x02
	"mute",
#define OPTION_MUTE		0x03
	"reference",
#define OPTION_REFERENCE	0x04
	"mono",
#define OPTION_MONO		0x05
	"stereo",
#define	OPTION_STEREO		0x06
	"sensitivity"
#define	OPTION_SENSITIVITY	0x07
};

#define OPTION_NONE		~0u
#define VALUE_NONE		~0u

struct opt_t {
	char *string;
	int option;
	int sign;
#define SIGN_NONE	0
#define SIGN_PLUS	1
#define SIGN_MINUS	-1
	u_int32_t value;
};

const char *onchar = "on";
#define ONCHAR_LEN	2
const char *offchar = "off";
#define OFFCHAR_LEN	3

static struct radio_info ri;

static int	parse_opt(char *, struct opt_t *);

static void	print_vars(int);
static void	do_ioctls(int, struct opt_t *, int);

static void	print_value(int);
static void	change_value(const struct opt_t);
static void	update_value(int, u_int *, u_int);

static void     warn_unsupported(int);
static void	usage(void);

static void	show_verbose(const char *, int);
static void	show_int_val(u_long, const char *, const char *, int);
static void	show_float_val(float, const char *, const char *, int);
static void	show_char_val(const char *, const char *, int);
static int	str_to_opt(const char *);
static u_long	str_to_long(char *, int);

/*
 * Control behavior of a FM tuner - set frequency, volume etc
 */
int
main(int argc, char **argv)
{
	struct opt_t opt;
	const char *radiodev = NULL;
	int rd = -1;
	int optchar;
	char *param = NULL;
	int show_vars = 0;
	int set_param = 0;
	int silent = 0;

	if (argc < 2) {
		usage();
		exit(1);
	}

	radiodev = getenv(RADIO_ENV);
	if (radiodev == NULL)
		radiodev = RADIODEVICE;

	while ((optchar = getopt(argc, argv, "af:nw:")) != -1) {
		switch (optchar) {
		case 'a':
			show_vars = 1;
			break;
		case 'f':
			radiodev = optarg;
			break;
		case 'n':
			silent = 1;
			break;
		case 'w':
			set_param = 1;
			param = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	rd = open(radiodev, O_RDONLY);
	if (rd < 0)
		err(1, "%s open error", radiodev);

	if (ioctl(rd, RIOCGINFO, &ri) < 0)
		err(1, "RIOCGINFO");

	if (argc > 1)
		if (parse_opt(*(argv + 1), &opt)) {
			show_verbose(varname[opt.option], silent);
			print_value(opt.option);
			free(opt.string);
			putchar('\n');
		}

	if (set_param)
		if (parse_opt(param, &opt))
			do_ioctls(rd, &opt, silent);

	if (show_vars)
		print_vars(silent);

	if (close(rd) < 0)
		warn("%s close error", radiodev);

	return 0;
}

static void
usage(void)
{
	const char *progname = getprogname();

	fprintf(stderr, "usage:\t%s %s\n\t%s %s\n\t%s %s\n\t%s %s\n",
		progname, "[-n] variable ...",
		progname, "[-n] -w name=value ...",
		progname, "[-n] -a",
		progname, "[-n] -f file");
	exit(1);
}

static void
show_verbose(const char *nick, int silent)
{
	if (!silent)
		printf("%s=", nick);
}

static void
warn_unsupported(int optval)
{
	warnx("driver does not support `%s'", varname[optval]);
}

static void
do_ioctls(int fd, struct opt_t *o, int silent)
{
	int oval;

	if (fd < 0 || o == NULL)
		return;

	if (o->option == OPTION_SEARCH && !(ri.caps & RADIO_CAPS_HW_SEARCH)) {
		warn_unsupported(o->option);
		return;
	}

	oval = o->option == OPTION_SEARCH ? OPTION_FREQUENCY : o->option;
	if (!silent)
		printf("%s: ", varname[oval]);

	print_value(o->option);
	printf(" -> ");

	if (o->option == OPTION_SEARCH) {

		if (ioctl(fd, RIOCSSRCH, &o->value) < 0) {
			warn("RIOCSSRCH");
			return;
		}

	} else {

		change_value(*o);
		if (ioctl(fd, RIOCSINFO, &ri) < 0) {
			warn("RIOCSINFO");
			return;
		}

	}

	if (ioctl(fd, RIOCGINFO, &ri) < 0) {
		warn("RIOCGINFO");
		return;
	}

	print_value(o->option);
	putchar('\n');
}

static void
change_value(const struct opt_t o)
{
	int unsupported = 0;

	if (o.value == VALUE_NONE)
		return;

	switch (o.option) {
	case OPTION_VOLUME:
		update_value(o.sign, (u_int *)&ri.volume, o.value);
		break;
	case OPTION_FREQUENCY:
		update_value(o.sign, (u_int *)&ri.freq, o.value);
		break;
	case OPTION_REFERENCE:
		if (ri.caps & RADIO_CAPS_REFERENCE_FREQ)
			update_value(o.sign, (u_int *)&ri.rfreq, o.value);
		else
			unsupported++;
		break;
	case OPTION_MONO:
		/* FALLTHROUGH */
	case OPTION_STEREO:
		if (ri.caps & RADIO_CAPS_SET_MONO)
			ri.stereo = o.option == OPTION_MONO ? !o.value : o.value;
		else
			unsupported++;
		break;
	case OPTION_SENSITIVITY:
		if (ri.caps & RADIO_CAPS_LOCK_SENSITIVITY)
			update_value(o.sign, (u_int *)&ri.lock, o.value);
		else
			unsupported++;
		break;
	case OPTION_MUTE:
		ri.mute = o.value;
		break;
	}

	if ( unsupported )
		warn_unsupported(o.option);
}

/*
 * Convert string to integer representation of a parameter
 */
static int
str_to_opt(const char *topt)
{
	int res, toptlen, varlen, len, varsize;

	if (topt == NULL || *topt == '\0')
		return OPTION_NONE;

	varsize = sizeof(varname) / sizeof(varname[0]);
	toptlen = strlen(topt);

	for (res = 0; res < varsize; res++) {
		varlen = strlen(varname[res]);
		len = toptlen > varlen ? toptlen : varlen;
		if (strncmp(topt, varname[res], len) == 0)
			return res;
	}

	warnx("name not found `%s'", topt);
	return OPTION_NONE;
}

static void
update_value(int sign, u_int *value, u_int update)
{
	switch (sign) {
	case SIGN_NONE:
		*value  = update;
		break;
	case SIGN_PLUS:
		*value += update;
		break;
	case SIGN_MINUS:
		*value -= update;
		break;
	}
}

/*
 * Convert string to unsigned integer
 */
static u_long
str_to_long(char *str, int optval)
{
	u_long val;

	if (str == NULL || *str == '\0')
		return VALUE_NONE;

	if (optval == OPTION_FREQUENCY)
		val = (u_long)1000 * atof(str);
	else
		val = (u_long)strtol(str, (char **)NULL, 10);

	return val;
}

/*
 * parse string s into struct opt_t
 * return true on success, false on failure
 */
static int
parse_opt(char *s, struct opt_t *o) {
	const char *badvalue = "bad value `%s'";
	char *topt = NULL;
	int slen, optlen;

	if (s == NULL || *s == '\0' || o == NULL)
		return 0;

	o->string = NULL;
	o->option = OPTION_NONE;
	o->value = VALUE_NONE;
	o->sign = SIGN_NONE;

	slen = strlen(s);
	optlen = strcspn(s, "=");

	/* Set only o->optval, the rest is missing */
	if (slen == optlen) {
		o->option = str_to_opt(s);
		return o->option == (int)OPTION_NONE ? 0 : 1;
	}

	if (optlen > slen - 2) {
		warnx(badvalue, s);
		return 0;
	}

	slen -= ++optlen;

	if ((topt = (char *)malloc(optlen)) == NULL) {
		warn("memory allocation error");
		return 0;
	}
	strlcpy(topt, s, optlen);

	if ((o->option = str_to_opt(topt)) == (int)OPTION_NONE) {
		free(topt);
		return 0;
	}
	o->string = topt;

	topt = &s[optlen];
	switch (*topt) {
	case '+':
	case '-':
		o->sign = (*topt == '+') ? SIGN_PLUS : SIGN_MINUS;
		o->value = str_to_long(&topt[1], o->option);
		break;
	case 'o':
		if (strncmp(topt, offchar,
			slen > OFFCHAR_LEN ? slen : OFFCHAR_LEN) == 0)
			o->value = 0;
		else if (strncmp(topt, onchar,
				slen > ONCHAR_LEN ? slen : ONCHAR_LEN) == 0)
				o->value = 1;
		break;
	case 'u':
		if (strncmp(topt, "up", slen > 2 ? slen : 2) == 0)
			o->value = 1;
		break;
	case 'd':
		if (strncmp(topt, "down", slen > 4 ? slen : 4) == 0)
			o->value = 0;
		break;
	default:
		if (*topt > 47 && *topt < 58)
			o->value = str_to_long(topt, o->option);
		break;
	}

	if (o->value == VALUE_NONE) {
		warnx(badvalue, topt);
		return 0;
	}

	return 1;
}

/*
 * Print current value of the parameter.
 */
static void
print_value(int optval)
{
	if (optval == (int)OPTION_NONE)
		return;

	switch (optval) {
	case OPTION_SEARCH:
		/* FALLTHROUGH */
	case OPTION_FREQUENCY:
		printf("%.2fMHz", (float)ri.freq / 1000.);
		break;
	case OPTION_REFERENCE:
		printf("%ukHz", ri.rfreq);
		break;
	case OPTION_SENSITIVITY:
		printf("%umkV", ri.lock);
		break;
	case OPTION_MUTE:
		printf(ri.mute ? onchar : offchar);
		break;
	case OPTION_MONO:
		printf(ri.stereo ? offchar : onchar);
		break;
	case OPTION_STEREO:
		printf(ri.stereo ? onchar : offchar);
		break;
	case OPTION_VOLUME:
	default:
		printf("%u", ri.volume);
		break;
	}
}

static void
show_int_val(u_long val, const char *nick, const char *append, int silent)
{
	show_verbose(nick, silent);
	printf("%lu%s\n", val, append);
}

static void
show_float_val(float val, const char *nick, const char *append, int silent)
{
	show_verbose(nick, silent);
	printf("%.2f%s\n", val, append);
}

static void
show_char_val(const char *val, const char *nick, int silent)
{
	show_verbose(nick, silent);
	printf("%s\n", val);
}

/*
 * Print all available parameters
 */
static void
print_vars(int silent)
{
	const char *delim;

	show_int_val(ri.volume, varname[OPTION_VOLUME], "", silent);
	show_float_val((float)ri.freq / 1000., varname[OPTION_FREQUENCY],
			"MHz", silent);
	show_char_val(ri.mute ? onchar : offchar, varname[OPTION_MUTE], silent);

	if (ri.caps & RADIO_CAPS_REFERENCE_FREQ)
		show_int_val(ri.rfreq, varname[OPTION_REFERENCE], "kHz", silent);
	if (ri.caps & RADIO_CAPS_LOCK_SENSITIVITY)
		show_int_val(ri.lock, varname[OPTION_SENSITIVITY], "mkV", silent);

	if (ri.caps & RADIO_CAPS_DETECT_SIGNAL) {
		show_verbose("signal", silent);
		printf("%s\n", ri.info & RADIO_INFO_SIGNAL ? onchar : offchar);
	}
	if (ri.caps & RADIO_CAPS_DETECT_STEREO) {
		show_verbose(varname[OPTION_STEREO], silent);
		printf("%s\n", ri.info & RADIO_INFO_STEREO ? onchar : offchar);
	}

	if (!silent)
		printf("card capabilities:");
	delim = "";
	if (ri.caps & RADIO_CAPS_DETECT_STEREO)
		printf("%s stereo detect", delim), delim=",";
	if (ri.caps & RADIO_CAPS_DETECT_SIGNAL)
		printf("%s signal detect", delim), delim=",";
	if (ri.caps & RADIO_CAPS_SET_MONO)
		printf("%s manageable mono/stereo", delim), delim=",";
	if (ri.caps & RADIO_CAPS_HW_SEARCH)
		printf("%s hardware search", delim), delim=",";
	if (ri.caps & RADIO_CAPS_HW_AFC)
		printf("%s hardware AFC", delim), delim=",";
	printf("\n");
}

/*	$NetBSD: keyboard.c,v 1.1.10.1 2000/07/07 09:53:42 hannken Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/ioctl.h>
#include <sys/time.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsconsio.h>
#include <err.h>
#include "wsconsctl.h"

static int kbtype;
static struct wskbd_bell_data bell;
static struct wskbd_bell_data dfbell;
static struct wscons_keymap mapdata[KS_NUMKEYCODES];
struct wskbd_map_data kbmap = { KS_NUMKEYCODES, mapdata };	/* used in map_parse.y
							   and in util.c */
static struct wskbd_keyrepeat_data repeat;
static struct wskbd_keyrepeat_data dfrepeat;
static int ledstate;
static kbd_t kbdencoding;

struct field keyboard_field_tab[] = {
    { "type",			&kbtype,	FMT_KBDTYPE,	FLG_RDONLY },
    { "bell.pitch",		&bell.pitch,	FMT_UINT,	FLG_MODIFY },
    { "bell.period",		&bell.period,	FMT_UINT,	FLG_MODIFY },
    { "bell.volume",		&bell.volume,	FMT_UINT,	FLG_MODIFY },
    { "bell.pitch.default",	&dfbell.pitch,	FMT_UINT,	FLG_MODIFY },
    { "bell.period.default",	&dfbell.period,	FMT_UINT,	FLG_MODIFY },
    { "bell.volume.default",	&dfbell.volume,	FMT_UINT,	FLG_MODIFY },
    { "map",			&kbmap,		FMT_KBMAP,	FLG_MODIFY|FLG_NOAUTO },
    { "repeat.del1",		&repeat.del1,	FMT_UINT,	FLG_MODIFY },
    { "repeat.deln",		&repeat.delN,	FMT_UINT,	FLG_MODIFY },
    { "repeat.del1.default",	&dfrepeat.del1,	FMT_UINT,	FLG_MODIFY },
    { "repeat.deln.default",	&dfrepeat.delN,	FMT_UINT,	FLG_MODIFY },
    { "ledstate",		&ledstate,	FMT_UINT,	0 },
    { "encoding",		&kbdencoding,	FMT_KBDENC,	FLG_MODIFY },
};

int keyboard_field_tab_len = sizeof(keyboard_field_tab)/
			      sizeof(keyboard_field_tab[0]);

void
keyboard_get_values(fd)
	int fd;
{
	if (field_by_value(&kbtype)->flags & FLG_GET)
		if (ioctl(fd, WSKBDIO_GTYPE, &kbtype) < 0)
			err(1, "WSKBDIO_GTYPE");

	bell.which = 0;
	if (field_by_value(&bell.pitch)->flags & FLG_GET)
		bell.which |= WSKBD_BELL_DOPITCH;
	if (field_by_value(&bell.period)->flags & FLG_GET)
		bell.which |= WSKBD_BELL_DOPERIOD;
	if (field_by_value(&bell.volume)->flags & FLG_GET)
		bell.which |= WSKBD_BELL_DOVOLUME;
	if (bell.which != 0 && ioctl(fd, WSKBDIO_GETBELL, &bell) < 0)
		err(1, "WSKBDIO_GETBELL");

	dfbell.which = 0;
	if (field_by_value(&dfbell.pitch)->flags & FLG_GET)
		dfbell.which |= WSKBD_BELL_DOPITCH;
	if (field_by_value(&dfbell.period)->flags & FLG_GET)
		dfbell.which |= WSKBD_BELL_DOPERIOD;
	if (field_by_value(&dfbell.volume)->flags & FLG_GET)
		dfbell.which |= WSKBD_BELL_DOVOLUME;
	if (dfbell.which != 0 &&
	    ioctl(fd, WSKBDIO_GETDEFAULTBELL, &dfbell) < 0)
		err(1, "WSKBDIO_GETDEFAULTBELL");

	if (field_by_value(&kbmap)->flags & FLG_GET) {
		kbmap.maplen = KS_NUMKEYCODES;
		if (ioctl(fd, WSKBDIO_GETMAP, &kbmap) < 0)
			err(1, "WSKBDIO_GETMAP");
	}

	repeat.which = 0;
	if (field_by_value(&repeat.del1)->flags & FLG_GET)
		repeat.which |= WSKBD_KEYREPEAT_DODEL1;
	if (field_by_value(&repeat.delN)->flags & FLG_GET)
		repeat.which |= WSKBD_KEYREPEAT_DODELN;
	if (repeat.which != 0 &&
	    ioctl(fd, WSKBDIO_GETKEYREPEAT, &repeat) < 0)
		err(1, "WSKBDIO_GETKEYREPEAT");

	dfrepeat.which = 0;
	if (field_by_value(&dfrepeat.del1)->flags & FLG_GET)
		dfrepeat.which |= WSKBD_KEYREPEAT_DODEL1;
	if (field_by_value(&dfrepeat.delN)->flags & FLG_GET)
		dfrepeat.which |= WSKBD_KEYREPEAT_DODELN;
	if (dfrepeat.which != 0 &&
	    ioctl(fd, WSKBDIO_GETKEYREPEAT, &dfrepeat) < 0)
		err(1, "WSKBDIO_GETKEYREPEAT");

	if (field_by_value(&ledstate)->flags & FLG_GET)
		if (ioctl(fd, WSKBDIO_GETLEDS, &ledstate) < 0)
			err(1, "WSKBDIO_GETLEDS");

	if (field_by_value(&kbdencoding)->flags & FLG_GET)
		if (ioctl(fd, WSKBDIO_GETENCODING, &kbdencoding) < 0)
			err(1, "WSKBDIO_GETENCODING");
}

void
keyboard_put_values(fd)
	int fd;
{
	bell.which = 0;
	if (field_by_value(&bell.pitch)->flags & FLG_SET)
		bell.which |= WSKBD_BELL_DOPITCH;
	if (field_by_value(&bell.period)->flags & FLG_SET)
		bell.which |= WSKBD_BELL_DOPERIOD;
	if (field_by_value(&bell.volume)->flags & FLG_SET)
		bell.which |= WSKBD_BELL_DOVOLUME;
	if (bell.which != 0 && ioctl(fd, WSKBDIO_SETBELL, &bell) < 0)
		err(1, "WSKBDIO_SETBELL");
	if (bell.which & WSKBD_BELL_DOPITCH)
		pr_field(field_by_value(&bell.pitch), " -> ");
	if (bell.which & WSKBD_BELL_DOPERIOD)
		pr_field(field_by_value(&bell.period), " -> ");
	if (bell.which & WSKBD_BELL_DOVOLUME)
		pr_field(field_by_value(&bell.volume), " -> ");

	dfbell.which = 0;
	if (field_by_value(&dfbell.pitch)->flags & FLG_SET)
		dfbell.which |= WSKBD_BELL_DOPITCH;
	if (field_by_value(&dfbell.period)->flags & FLG_SET)
		dfbell.which |= WSKBD_BELL_DOPERIOD;
	if (field_by_value(&dfbell.volume)->flags & FLG_SET)
		dfbell.which |= WSKBD_BELL_DOVOLUME;
	if (dfbell.which != 0 &&
	    ioctl(fd, WSKBDIO_SETDEFAULTBELL, &dfbell) < 0)
		err(1, "WSKBDIO_SETDEFAULTBELL");
	if (dfbell.which & WSKBD_BELL_DOPITCH)
		pr_field(field_by_value(&dfbell.pitch), " -> ");
	if (dfbell.which & WSKBD_BELL_DOPERIOD)
		pr_field(field_by_value(&dfbell.period), " -> ");
	if (dfbell.which & WSKBD_BELL_DOVOLUME)
		pr_field(field_by_value(&dfbell.volume), " -> ");

	if (field_by_value(&kbmap)->flags & FLG_SET) {
		if (ioctl(fd, WSKBDIO_SETMAP, &kbmap) < 0)
			err(1, "WSKBDIO_SETMAP");
		pr_field(field_by_value(&kbmap), " -> ");
	}

	repeat.which = 0;
	if (field_by_value(&repeat.del1)->flags & FLG_SET)
		repeat.which |= WSKBD_KEYREPEAT_DODEL1;
	if (field_by_value(&repeat.delN)->flags & FLG_SET)
		repeat.which |= WSKBD_KEYREPEAT_DODELN;
	if (repeat.which != 0 &&
	    ioctl(fd, WSKBDIO_SETKEYREPEAT, &repeat) < 0)
		err(1, "WSKBDIO_SETKEYREPEAT");
	if (repeat.which & WSKBD_KEYREPEAT_DODEL1)
		pr_field(field_by_value(&repeat.del1), " -> ");
	if (repeat.which & WSKBD_KEYREPEAT_DODELN)
		pr_field(field_by_value(&repeat.delN), " -> ");

	dfrepeat.which = 0;
	if (field_by_value(&dfrepeat.del1)->flags & FLG_SET)
		dfrepeat.which |= WSKBD_KEYREPEAT_DODEL1;
	if (field_by_value(&dfrepeat.delN)->flags & FLG_SET)
		dfrepeat.which |= WSKBD_KEYREPEAT_DODELN;
	if (dfrepeat.which != 0 &&
	    ioctl(fd, WSKBDIO_SETDEFAULTKEYREPEAT, &dfrepeat) < 0)
		err(1, "WSKBDIO_SETDEFAULTKEYREPEAT");
	if (dfrepeat.which &WSKBD_KEYREPEAT_DODEL1)
		pr_field(field_by_value(&dfrepeat.del1), " -> ");
	if (dfrepeat.which & WSKBD_KEYREPEAT_DODELN)
		pr_field(field_by_value(&dfrepeat.delN), " -> ");

	if (field_by_value(&ledstate)->flags & FLG_SET) {
		if (ioctl(fd, WSKBDIO_SETLEDS, &ledstate) < 0)
			err(1, "WSKBDIO_SETLEDS");
		pr_field(field_by_value(&ledstate), " -> ");
	}

	if (field_by_value(&kbdencoding)->flags & FLG_SET) {
		if (ioctl(fd, WSKBDIO_SETENCODING, &kbdencoding) < 0)
			err(1, "WSKBDIO_SETENCODING");
		pr_field(field_by_value(&kbdencoding), " -> ");
	}
}

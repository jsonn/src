/*	$NetBSD: pwd_gensalt.c,v 1.5.2.2 2002/02/26 22:09:42 he Exp $	*/

/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from OpenBSD: pwd_gensalt.c,v 1.9 1998/07/05 21:08:32 provos Exp
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: pwd_gensalt.c,v 1.5.2.2 2002/02/26 22:09:42 he Exp $");
#endif /* not lint */

#include <sys/syslimits.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <util.h>
#include <time.h>
#include <pwd.h>

#include "extern.h"

static unsigned char itoa64[] =	 /* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void to64(char *s, long v, int n);

int
pwd_gensalt(char *salt, int max, struct passwd *pwd, char type)
{
	char option[LINE_MAX], *next, *now, *cipher, grpkey[LINE_MAX];
	int rounds;
	struct group *grp;

	*salt = '\0';

	switch (type) {
	case 'y':
	        cipher = "ypcipher";
		break;
	case 'l':
	default:
	        cipher = "localcipher";
		break;
	}

	pw_getconf(option, sizeof(option), pwd->pw_name, cipher);

	/* Try to find an entry for the group */
	if (*option == 0) {
		if ((grp = getgrgid(pwd->pw_gid)) != NULL) {
			snprintf(grpkey, sizeof(grpkey), ":%s", grp->gr_name);
			pw_getconf(option, sizeof(option), grpkey, cipher);
		}
		if (*option == 0)
		        pw_getconf(option, sizeof(option), "default", cipher);
	}

	srandom((int)time(NULL));
	next = option;
	now = strsep(&next, ",");
	if (strcmp(now, "old") == 0) {
		if (max < 3)
			return (0);
		to64(&salt[0], random(), 2);
		salt[2] = '\0';
	} else if (strcmp(now, "newsalt") == 0) {
		rounds = atol(next);
		if (max < 10)
			return (0);
		/* Check rounds, 24 bit is max */
		if (rounds < 7250)
			rounds = 7250;
		else if (rounds > 0xffffff)
		        rounds = 0xffffff;
		salt[0] = _PASSWORD_EFMT1;
		to64(&salt[1], (u_int32_t)rounds, 4);
		to64(&salt[5], random(), 4);
		salt[9] = '\0';
	} else if (strcmp(now, "md5") == 0) {
		if (max < 13)  /* $1$8salt$\0 */
			return (0);
		salt[0] = _PASSWORD_NONDES;
		salt[1] = '1';
		salt[2] = '$';
		to64(&salt[3], random(), 4);
		to64(&salt[7], random(), 4);
		salt[11] = '$';
		salt[12] = '\0';
#if 0
	} else if (strcmp(now, "blowfish")) {
		rounds = atoi(next);
		if (rounds < 4)
			rounds = 4;
		strncpy(salt, bcrypt_gensalt(rounds), max - 1);
		salt[max - 1] = 0;
#endif
	} else {
		strcpy(salt, ":");
		warnx("Unknown option %s.", now);
	}

	return (1);
}

static void
to64(char *s, long v, int n)
{

	while (--n >= 0) {
		*s++ = itoa64[v & 0x3f];
		v >>= 6;
	}
}

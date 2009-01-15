/*	$NetBSD: rune.c,v 1.30.12.1 2009/01/15 03:24:08 snj Exp $	*/

/*-
 * Copyright (c)1999 Citrus Project,
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

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)rune.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: rune.c,v 1.30.12.1 2009/01/15 03:24:08 snj Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "citrus_module.h"
#include "citrus_ctype.h"

#include "bsdctype.h"
#include "rune.h"
#include "rune_local.h"

static int readrange __P((_RuneLocale *, _RuneRange *, _FileRuneRange *, void *, FILE *));
static void _freeentry __P((_RuneRange *));
static void _wctype_init __P((_RuneLocale *rl));

static int
readrange(_RuneLocale *rl, _RuneRange *rr, _FileRuneRange *frr, void *lastp,
	FILE *fp)
{
	uint32_t i;
	_RuneEntry *re;
	_FileRuneEntry fre;

	_DIAGASSERT(rl != NULL);
	_DIAGASSERT(rr != NULL);
	_DIAGASSERT(frr != NULL);
	_DIAGASSERT(lastp != NULL);
	_DIAGASSERT(fp != NULL);

	re = (_RuneEntry *)rl->rl_variable;

	rr->rr_nranges = ntohl(frr->frr_nranges);
	if (rr->rr_nranges == 0) {
		rr->rr_rune_ranges = NULL;
		return 0;
	}

	rr->rr_rune_ranges = re;
	for (i = 0; i < rr->rr_nranges; i++) {
		if (fread(&fre, sizeof(fre), 1, fp) != 1)
			return -1;

		re->re_min = ntohl((u_int32_t)fre.fre_min);
		re->re_max = ntohl((u_int32_t)fre.fre_max);
		re->re_map = ntohl((u_int32_t)fre.fre_map);
		re++;

		if ((void *)re > lastp)
			return -1;
	}
	rl->rl_variable = re;
	return 0;
}

static int
readentry(_RuneRange *rr, FILE *fp)
{
	_RuneEntry *re;
	size_t l, i, j;
	int error;

	_DIAGASSERT(rr != NULL);
	_DIAGASSERT(fp != NULL);

	re = rr->rr_rune_ranges;
	for (i = 0; i < rr->rr_nranges; i++) {
		if (re[i].re_map != 0) {
			re[i].re_rune_types = NULL;
			continue;
		}

		l = re[i].re_max - re[i].re_min + 1;
		re[i].re_rune_types = malloc(l * sizeof(_RuneType));
		if (!re[i].re_rune_types) {
			error = ENOMEM;
			goto fail;
		}
		memset(re[i].re_rune_types, 0, l * sizeof(_RuneType));

		if (fread(re[i].re_rune_types, sizeof(_RuneType), l, fp) != l)
			goto fail2;

		for (j = 0; j < l; j++)
			re[i].re_rune_types[j] = ntohl(re[i].re_rune_types[j]);
	}
	return 0;

fail:
	for (j = 0; j < i; j++) {
		free(re[j].re_rune_types);
		re[j].re_rune_types = NULL;
	}
	return error;
fail2:
	for (j = 0; j <= i; j++) {
		free(re[j].re_rune_types);
		re[j].re_rune_types = NULL;
	}
	return errno;
}

/* XXX: temporary implementation */
static void
find_codeset(_RuneLocale *rl)
{
	char *top, *codeset, *tail, *ep;

	/* end of rl_variable region */
	ep = (char *)rl->rl_variable;
	ep += rl->rl_variable_len;
	rl->rl_codeset = NULL;
	if (!(top = strstr(rl->rl_variable, _RUNE_CODESET)))
		return;
	tail = strpbrk(top, " \t");
	codeset = top + sizeof(_RUNE_CODESET) - 1;
	if (tail) {
		*top = *tail;
		*tail = '\0';
		rl->rl_codeset = strdup(codeset);
		strlcpy(top + 1, tail + 1, (unsigned)(ep - (top + 1)));
	} else {
		*top = '\0';
		rl->rl_codeset = strdup(codeset);
	}
}

void
_freeentry(_RuneRange *rr)
{
	_RuneEntry *re;
	uint32_t i;

	_DIAGASSERT(rr != NULL);

	re = rr->rr_rune_ranges;
	for (i = 0; i < rr->rr_nranges; i++) {
		if (re[i].re_rune_types)
			free(re[i].re_rune_types);
		re[i].re_rune_types = NULL;
	}
}

void
_wctype_init(_RuneLocale *rl)
{
	memcpy(&rl->rl_wctype, &_DefaultRuneLocale.rl_wctype,
	       sizeof(rl->rl_wctype));
}


_RuneLocale *
_Read_RuneMagi(fp)
	FILE *fp;
{
	/* file */
	_FileRuneLocale frl;
	/* host data */
	char *hostdata;
	size_t hostdatalen;
	void *lastp;
	_RuneLocale *rl;
	struct stat sb;
	int x;

	_DIAGASSERT(fp != NULL);

	if (fstat(fileno(fp), &sb) < 0)
		return NULL;

	if (sb.st_size < sizeof(_FileRuneLocale))
		return NULL;
	/* XXX more validation? */

	/* Someone might have read the magic number once already */
	rewind(fp);

	if (fread(&frl, sizeof(frl), 1, fp) != 1)
		return NULL;
	if (memcmp(frl.frl_magic, _RUNE_MAGIC_1, sizeof(frl.frl_magic)))
		return NULL;

	hostdatalen = sizeof(*rl) + ntohl((u_int32_t)frl.frl_variable_len) +
	    ntohl(frl.frl_runetype_ext.frr_nranges) * sizeof(_RuneEntry) +
	    ntohl(frl.frl_maplower_ext.frr_nranges) * sizeof(_RuneEntry) +
	    ntohl(frl.frl_mapupper_ext.frr_nranges) * sizeof(_RuneEntry);

	if ((hostdata = malloc(hostdatalen)) == NULL)
		return NULL;
	memset(hostdata, 0, hostdatalen);
	lastp = hostdata + hostdatalen;

	rl = (_RuneLocale *)(void *)hostdata;
	rl->rl_variable = rl + 1;

	memcpy(rl->rl_magic, frl.frl_magic, sizeof(rl->rl_magic));
	memcpy(rl->rl_encoding, frl.frl_encoding, sizeof(rl->rl_encoding));

	rl->rl_invalid_rune = ntohl((u_int32_t)frl.frl_invalid_rune);
	rl->rl_variable_len = ntohl((u_int32_t)frl.frl_variable_len);

	for (x = 0; x < _CACHED_RUNES; ++x) {
		rl->rl_runetype[x] = ntohl(frl.frl_runetype[x]);

		/* XXX assumes rune_t = u_int32_t */
		rl->rl_maplower[x] = ntohl((u_int32_t)frl.frl_maplower[x]);
		rl->rl_mapupper[x] = ntohl((u_int32_t)frl.frl_mapupper[x]);
	}

	if (readrange(rl, &rl->rl_runetype_ext, &frl.frl_runetype_ext, lastp, fp))
	{
		free(hostdata);
		return NULL;
	}
	if (readrange(rl, &rl->rl_maplower_ext, &frl.frl_maplower_ext, lastp, fp))
	{
		free(hostdata);
		return NULL;
	}
	if (readrange(rl, &rl->rl_mapupper_ext, &frl.frl_mapupper_ext, lastp, fp))
	{
		free(hostdata);
		return NULL;
	}

	if (readentry(&rl->rl_runetype_ext, fp) != 0) {
		free(hostdata);
		return NULL;
	}

	if ((u_int8_t *)rl->rl_variable + rl->rl_variable_len >
	    (u_int8_t *)lastp) {
		_freeentry(&rl->rl_runetype_ext);
		free(hostdata);
		return NULL;
	}
	if (rl->rl_variable_len == 0)
		rl->rl_variable = NULL;
	if (rl->rl_variable == NULL ||
	    fread(rl->rl_variable, rl->rl_variable_len, 1, fp) != 1) {
		_freeentry(&rl->rl_runetype_ext);
		free(hostdata);
		return NULL;
	}
	find_codeset(rl);
	_wctype_init(rl);

	/* error if we have junk at the tail */
	if (ftell(fp) != sb.st_size) {
		_freeentry(&rl->rl_runetype_ext);
		free(hostdata);
		return NULL;
	}

	return(rl);
}

void
_NukeRune(rl)
	_RuneLocale *rl;
{

	_DIAGASSERT(rl != NULL);

	if (rl != &_DefaultRuneLocale) {
		_freeentry(&rl->rl_runetype_ext);
		if (rl->rl_codeset)
			free(__UNCONST(rl->rl_codeset));
		if (rl->rl_citrus_ctype)
			_citrus_ctype_close(rl->rl_citrus_ctype);
		free(__UNCONST(rl->rl_ctype_tab));
		free(__UNCONST(rl->rl_tolower_tab));
		free(__UNCONST(rl->rl_toupper_tab));
		free(rl);
	}
}

/*
 * read in old LC_CTYPE declaration file, convert into runelocale info
 */
#define _CTYPE_PRIVATE
#include <limits.h>
#include <ctype.h>

_RuneLocale *
_Read_CTypeAsRune(fp)
	FILE *fp;
{
	char id[sizeof(_CTYPE_ID) - 1];
	u_int32_t i, len;
	u_int8_t *new_ctype = NULL;
	int16_t *new_toupper = NULL, *new_tolower = NULL;
	/* host data */
	char *hostdata = NULL;
	size_t hostdatalen;
	_RuneLocale *rl;
	struct stat sb;
	int x;

	_DIAGASSERT(fp != NULL);

	if (fstat(fileno(fp), &sb) < 0)
		return NULL;

	if (sb.st_size < sizeof(id))
		return NULL;
	/* XXX more validation? */

	/* Someone might have read the magic number once already */
	rewind(fp);

	if (fread(id, sizeof(id), 1, fp) != 1)
		goto bad;
	if (memcmp(id, _CTYPE_ID, sizeof(id)) != 0)
		goto bad;

	if (fread(&i, sizeof(u_int32_t), 1, fp) != 1) 
		goto bad;
	if ((i = ntohl(i)) != _CTYPE_REV)
		goto bad;

	if (fread(&len, sizeof(u_int32_t), 1, fp) != 1)
		goto bad;
	if ((len = ntohl(len)) != _CTYPE_NUM_CHARS)
		goto bad;

	if ((new_ctype = malloc(sizeof(u_int8_t) * (1 + len))) == NULL ||
	    (new_toupper = malloc(sizeof(int16_t) * (1 + len))) == NULL ||
	    (new_tolower = malloc(sizeof(int16_t) * (1 + len))) == NULL)
		goto bad;
	new_ctype[0] = 0;
	if (fread(&new_ctype[1], sizeof(u_int8_t), len, fp) != len)
		goto bad;
	new_toupper[0] = EOF;
	if (fread(&new_toupper[1], sizeof(int16_t), len, fp) != len)
		goto bad;
	new_tolower[0] = EOF;
	if (fread(&new_tolower[1], sizeof(int16_t), len, fp) != len)
		goto bad;

	hostdatalen = sizeof(*rl);

	if ((hostdata = malloc(hostdatalen)) == NULL)
		goto bad;
	memset(hostdata, 0, hostdatalen);
	rl = (_RuneLocale *)(void *)hostdata;
	rl->rl_variable = NULL;

	memcpy(rl->rl_magic, _RUNE_MAGIC_1, sizeof(rl->rl_magic));
	memcpy(rl->rl_encoding, "NONE", 4);

	rl->rl_invalid_rune = _DefaultRuneLocale.rl_invalid_rune;	/*XXX*/
	rl->rl_variable_len = 0;

	for (x = 0; x < _CACHED_RUNES; ++x) {
		if ((uint32_t) x > len)
			continue;

		/*
		 * TWEAKS!
		 * - old locale file declarations do not have proper _B
		 *   in many cases.
		 * - isprint() declaration in ctype.h incorrectly uses _B.
		 *   _B means "isprint but !isgraph", not "isblank" with the
		 *   declaration.
		 * - _X and _CTYPE_X have negligible difference in meaning.
		 * - we don't set digit value, fearing that it would be
		 *   too much of hardcoding.  we may need to revisit it.
		 */

		if (new_ctype[1 + x] & _U)
			rl->rl_runetype[x] |= _CTYPE_U;
		if (new_ctype[1 + x] & _L)
			rl->rl_runetype[x] |= _CTYPE_L;
		if (new_ctype[1 + x] & _N)
			rl->rl_runetype[x] |= _CTYPE_D;
		if (new_ctype[1 + x] & _S)
			rl->rl_runetype[x] |= _CTYPE_S;
		if (new_ctype[1 + x] & _P)
			rl->rl_runetype[x] |= _CTYPE_P;
		if (new_ctype[1 + x] & _C)
			rl->rl_runetype[x] |= _CTYPE_C;
		/* derived flag bits, duplicate of ctype.h */
		if (new_ctype[1 + x] & (_U | _L))
			rl->rl_runetype[x] |= _CTYPE_A;
		if (new_ctype[1 + x] & (_N | _X))
			rl->rl_runetype[x] |= _CTYPE_X;
		if (new_ctype[1 + x] & (_P|_U|_L|_N))
			rl->rl_runetype[x] |= _CTYPE_G;
		/* we don't really trust _B in the file.  see above. */
		if (new_ctype[1 + x] & _B)
			rl->rl_runetype[x] |= _CTYPE_B;
		if ((new_ctype[1 + x] & (_P|_U|_L|_N|_B)) || x == ' ')
			rl->rl_runetype[x] |= (_CTYPE_R | _CTYPE_SW1);
		if (x == ' ' || x == '\t')
			rl->rl_runetype[x] |= _CTYPE_B;

		/* XXX may fail on non-8bit encoding only */
		rl->rl_mapupper[x] = ntohs(new_toupper[1 + x]);
		rl->rl_maplower[x] = ntohs(new_tolower[1 + x]);
	}

	_wctype_init(rl);

	/*
	 * __runetable_to_netbsd_ctype() will be called from
	 * setrunelocale.c:_newrunelocale(), and fill old ctype table.
	 */

	free(new_ctype);
	free(new_toupper);
	free(new_tolower);
	return(rl);

bad:
	if (new_ctype)
		free(new_ctype);
	if (new_toupper)
		free(new_toupper);
	if (new_tolower)
		free(new_tolower);
	return NULL;
}

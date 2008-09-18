/*	$NetBSD: funcs.c,v 1.9.12.1 2008/09/18 04:44:42 wrstuden Exp $	*/

/*
 * Copyright (c) Christos Zoulas 2003.
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "file.h"
#include "magic.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(HAVE_WCHAR_H)
#include <wchar.h>
#endif
#if defined(HAVE_WCTYPE_H)
#include <wctype.h>
#endif
#if defined(HAVE_LIMITS_H)
#include <limits.h>
#endif

#ifndef	lint
#if 0
FILE_RCSID("@(#)$File: funcs.c,v 1.44 2008/07/16 18:00:57 christos Exp $")
#else
__RCSID("$NetBSD: funcs.c,v 1.9.12.1 2008/09/18 04:44:42 wrstuden Exp $");
#endif
#endif	/* lint */

#ifndef SIZE_MAX
#define SIZE_MAX	((size_t)~0)
#endif

/*
 * Like printf, only we append to a buffer.
 */
protected int
file_vprintf(struct magic_set *ms, const char *fmt, va_list ap)
{
	int len;
	char *buf, *newstr;

	len = vasprintf(&buf, fmt, ap);
	if (len < 0)
		goto out;

	if (ms->o.buf != NULL) {
		len = asprintf(&newstr, "%s%s", ms->o.buf, buf);
		free(buf);
		if (len < 0)
			goto out;
		free(ms->o.buf);
		buf = newstr;
	}
	ms->o.buf = buf;
	return 0;
out:
	file_error(ms, errno, "vasprintf failed");
	return -1;
}

protected int
file_printf(struct magic_set *ms, const char *fmt, ...)
{
	int rv;
	va_list ap;

	va_start(ap, fmt);
	rv = file_vprintf(ms, fmt, ap);
	va_end(ap);
	return rv;
}

/*
 * error - print best error message possible
 */
/*VARARGS*/
private void
file_error_core(struct magic_set *ms, int error, const char *f, va_list va,
    uint32_t lineno)
{
	/* Only the first error is ok */
	if (ms->haderr)
		return;
	if (lineno != 0) {
		free(ms->o.buf);
		ms->o.buf = NULL;
		file_printf(ms, "line %u: ", lineno);
	}
        file_vprintf(ms, f, va);
	if (error > 0)
		file_printf(ms, " (%s)", strerror(error));
	ms->haderr++;
	ms->error = error;
}

/*VARARGS*/
protected void
file_error(struct magic_set *ms, int error, const char *f, ...)
{
	va_list va;
	va_start(va, f);
	file_error_core(ms, error, f, va, 0);
	va_end(va);
}

/*
 * Print an error with magic line number.
 */
/*VARARGS*/
protected void
file_magerror(struct magic_set *ms, const char *f, ...)
{
	va_list va;
	va_start(va, f);
	file_error_core(ms, 0, f, va, ms->line);
	va_end(va);
}

protected void
file_oomem(struct magic_set *ms, size_t len)
{
	file_error(ms, errno, "cannot allocate %zu bytes", len);
}

protected void
file_badseek(struct magic_set *ms)
{
	file_error(ms, errno, "error seeking");
}

protected void
file_badread(struct magic_set *ms)
{
	file_error(ms, errno, "error reading");
}

#ifndef COMPILE_ONLY
protected int
file_buffer(struct magic_set *ms, int fd, const char *inname, const void *buf,
    size_t nb)
{
	int m;
	int mime = ms->flags & MAGIC_MIME;
	const unsigned char *ubuf = CAST(const unsigned char *, buf);

	if (nb == 0) {
		if ((!mime || (mime & MAGIC_MIME_TYPE)) &&
		    file_printf(ms, mime ? "application/x-empty" :
		    "empty") == -1)
			return -1;
		return 1;
	} else if (nb == 1) {
		if ((!mime || (mime & MAGIC_MIME_TYPE)) &&
		    file_printf(ms, mime ? "application/octet-stream" :
		    "very short file (no magic)") == -1)
			return -1;
		return 1;
	}

#ifdef __EMX__
	if ((ms->flags & MAGIC_NO_CHECK_APPTYPE) == 0 && inname) {
		switch (file_os2_apptype(ms, inname, buf, nb)) {
		case -1:
			return -1;
		case 0:
			break;
		default:
			return 1;
		}
	}
#endif

	/* try compression stuff */
	if ((ms->flags & MAGIC_NO_CHECK_COMPRESS) != 0 ||
	    (m = file_zmagic(ms, fd, inname, ubuf, nb)) == 0) {
	    /* Check if we have a tar file */
	    if ((ms->flags & MAGIC_NO_CHECK_TAR) != 0 ||
		(m = file_is_tar(ms, ubuf, nb)) == 0) {
		/* try tests in /etc/magic (or surrogate magic file) */
		if ((ms->flags & MAGIC_NO_CHECK_SOFT) != 0 ||
		    (m = file_softmagic(ms, ubuf, nb, BINTEST)) == 0) {
		    /* try known keywords, check whether it is ASCII */
		    if ((ms->flags & MAGIC_NO_CHECK_ASCII) != 0 ||
			(m = file_ascmagic(ms, ubuf, nb)) == 0) {
			/* abandon hope, all ye who remain here */
			if ((!mime || (mime & MAGIC_MIME_TYPE)) &&
			    file_printf(ms, mime ? "application/octet-stream" :
				"data") == -1)
				return -1;
			m = 1;
		    }
		}
	    }
	}
#ifdef BUILTIN_ELF
	if ((ms->flags & MAGIC_NO_CHECK_ELF) == 0 && m == 1 &&
	    nb > 5 && fd != -1) {
		/*
		 * We matched something in the file, so this *might*
		 * be an ELF file, and the file is at least 5 bytes
		 * long, so if it's an ELF file it has at least one
		 * byte past the ELF magic number - try extracting
		 * information from the ELF headers that cannot easily
		 * be extracted with rules in the magic file.
		 */
		(void)file_tryelf(ms, fd, ubuf, nb);
	}
#endif
	return m;
}
#endif

protected int
file_reset(struct magic_set *ms)
{
	if (ms->mlist == NULL) {
		file_error(ms, 0, "no magic files loaded");
		return -1;
	}
	ms->o.buf = NULL;
	ms->haderr = 0;
	ms->error = -1;
	return 0;
}

#define OCTALIFY(n, o)	\
	/*LINTED*/ \
	(void)(*(n)++ = '\\', \
	*(n)++ = (((uint32_t)*(o) >> 6) & 3) + '0', \
	*(n)++ = (((uint32_t)*(o) >> 3) & 7) + '0', \
	*(n)++ = (((uint32_t)*(o) >> 0) & 7) + '0', \
	(o)++)

protected const char *
file_getbuffer(struct magic_set *ms)
{
	char *pbuf, *op, *np;
	size_t psize, len;

	if (ms->haderr)
		return NULL;

	if (ms->flags & MAGIC_RAW)
		return ms->o.buf;

	/* * 4 is for octal representation, + 1 is for NUL */
	len = strlen(ms->o.buf);
	if (len > (SIZE_MAX - 1) / 4) {
		file_oomem(ms, len);
		return NULL;
	}
	psize = len * 4 + 1;
	if ((pbuf = CAST(char *, realloc(ms->o.pbuf, psize))) == NULL) {
		file_oomem(ms, psize);
		return NULL;
	}
	ms->o.pbuf = pbuf;

#if defined(HAVE_WCHAR_H) && defined(HAVE_MBRTOWC) && defined(HAVE_WCWIDTH)
	{
		mbstate_t state;
		wchar_t nextchar;
		int mb_conv = 1;
		size_t bytesconsumed;
		char *eop;
		(void)memset(&state, 0, sizeof(mbstate_t));

		np = ms->o.pbuf;
		op = ms->o.buf;
		eop = op + len;

		while (op < eop) {
			bytesconsumed = mbrtowc(&nextchar, op,
			    (size_t)(eop - op), &state);
			if (bytesconsumed == (size_t)(-1) ||
			    bytesconsumed == (size_t)(-2)) {
				mb_conv = 0;
				break;
			}

			if (iswprint(nextchar)) {
				(void)memcpy(np, op, bytesconsumed);
				op += bytesconsumed;
				np += bytesconsumed;
			} else {
				while (bytesconsumed-- > 0)
					OCTALIFY(np, op);
			}
		}
		*np = '\0';

		/* Parsing succeeded as a multi-byte sequence */
		if (mb_conv != 0)
			return ms->o.pbuf;
	}
#endif

	for (np = ms->o.pbuf, op = ms->o.buf; *op; op++) {
		if (isprint((unsigned char)*op)) {
			*np++ = *op;	
		} else {
			OCTALIFY(np, op);
		}
	}
	*np = '\0';
	return ms->o.pbuf;
}

protected int
file_check_mem(struct magic_set *ms, unsigned int level)
{
	size_t len;

	if (level >= ms->c.len) {
		len = (ms->c.len += 20) * sizeof(*ms->c.li);
		ms->c.li = CAST(struct level_info *, (ms->c.li == NULL) ?
		    malloc(len) :
		    realloc(ms->c.li, len));
		if (ms->c.li == NULL) {
			file_oomem(ms, len);
			return -1;
		}
	}
	ms->c.li[level].got_match = 0;
#ifdef ENABLE_CONDITIONALS
	ms->c.li[level].last_match = 0;
	ms->c.li[level].last_cond = COND_NONE;
#endif /* ENABLE_CONDITIONALS */
	return 0;
}

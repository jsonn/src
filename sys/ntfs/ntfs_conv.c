/*	$NetBSD: ntfs_conv.c,v 1.1.6.1 2002/01/10 20:04:31 thorpej Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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

/*
 * File name recode stuff.
 *
 * The utf-8 routines were derived from basesrc/lib/libc/locale/utf2.c.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ntfs_conv.c,v 1.1.6.1 2002/01/10 20:04:31 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#if defined(__FreeBSD__)
#include <machine/clock.h>
#endif

#include <miscfs/specfs/specdev.h>

/* #define NTFS_DEBUG 1 */
#include <ntfs/ntfs.h>
#include <ntfs/ntfsmount.h>
#include <ntfs/ntfs_inode.h>
#include <ntfs/ntfs_vfsops.h>
#include <ntfs/ntfs_subr.h>
#include <ntfs/ntfs_compr.h>
#include <ntfs/ntfs_ihash.h>

/* UTF-8 encoding stuff */

static const int _utf_count[16] = {
        1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 2, 2, 3, 0,
};

/*
 * Read one wide character off the string, shift the string pointer
 * and return the character.
 */
wchar
ntfs_utf8_wget(const char **str)
{
	int c;
	wchar rune = 0;
	const char *s = *str;

	c = _utf_count[(s[0] >> 4) & 0xf];
	if (c == 0) {
		c = 1;
		goto encoding_error;
	}

	switch (c) {
	case 1:
		rune = s[0] & 0xff;
		break;
	case 2:
		if ((s[1] & 0xc0) != 0x80)
			goto encoding_error;
		rune = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
		break;
	case 3:
		if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80)
			goto encoding_error;
		rune = ((s[0] & 0x1F) << 12) | ((s[1] & 0x3F) << 6)
		    | (s[2] & 0x3F);
		break;
	}

encoding_error:
	*str = *str + c;
	return rune;
}

/*
 * Encode wide character and write it to the string. 'n' specifies
 * how much space there is in the string. Returns number of bytes written
 * to the target string.
 */
int
ntfs_utf8_wput(char *s, size_t n, wchar wc)
{
        if (wc & 0xf800) {
                if (n < 3) {
                        /* bound check failure */
			ddprintf(("ntfs_utf8_wput: need 3 bytes\n"));
                        return 0;
                }

                s[0] = 0xE0 | ((wc >> 12) & 0x0F);
                s[1] = 0x80 | ((wc >> 6) & 0x3F);
                s[2] = 0x80 | ((wc) & 0x3F);
                return 3;
        } else {
                if (wc & 0x0780) {
                        if (n < 2) {
                                /* bound check failure */
				ddprintf(("ntfs_utf8_wput: need 2 bytes\n"));
                                return 0;
                        }

                        s[0] = 0xC0 | ((wc >> 6) & 0x1F);
                        s[1] = 0x80 | ((wc) & 0x3F);
                        return 2;
                } else {
                        if (n < 1) {
                                /* bound check failure */
				ddprintf(("ntfs_utf8_wput: need 1 byte\n"));
                                return 0;
                        }

                        s[0] = wc;
                        return 1;
                }
        }
}

/*
 * Compare two wide characters, returning 1, 0, -1 if the first is
 * bigger, equal or lower than the second.
 */
int
ntfs_utf8_wcmp(wchar wc1, wchar wc2)
{
	/* no conversions needed for utf8 */

	if (wc1 == wc2)
		return 0;
	else
		return (int) wc1 - (int) wc2; 
}

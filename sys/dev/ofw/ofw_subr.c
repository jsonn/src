/*	$NetBSD: ofw_subr.c,v 1.6.8.1 2002/01/10 19:56:23 thorpej Exp $	*/

/*
 * Copyright 1998
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ofw_subr.c,v 1.6.8.1 2002/01/10 19:56:23 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <dev/ofw/openfirm.h>

#define	OFW_MAX_STACK_BUF_SIZE	256
#define	OFW_PATH_BUF_SIZE	512

/*
 * int of_decode_int(p)
 *
 * This routine converts OFW encoded-int datums
 * into the integer format of the host machine.
 *
 * It is primarily used to convert integer properties
 * returned by the OF_getprop routine.
 *
 * Arguments:
 *	p		pointer to unsigned char array which is an
 *			OFW-encoded integer.
 *
 * Return Value:
 *	Decoded integer value of argument p.
 *
 * Side Effects:
 *	None.
 */
int
of_decode_int(p)
	const unsigned char *p;
{
	unsigned int i = *p++ << 8;
	i = (i + *p++) << 8;
	i = (i + *p++) << 8;
	return (i + *p);
}

/*
 * int of_compatible(phandle, strings)
 *
 * This routine checks an OFW node's "compatible" entry to see if
 * it matches any of the provided strings.
 *
 * It should be used when determining whether a driver can drive
 * a partcular device.
 *
 * Arguments:
 *	phandle		OFW phandle of device to be checked for
 *			compatibility.
 *	strings		Array of containing expected "compatibility"
 *			property values, presence of any of which
 *			indicates compatibility.
 *
 * Return Value:
 *	-1 if none of the strings are found in phandle's "compatiblity"
 *	property, or the index of the string in "strings" of the first
 *	string found in phandle's "compatibility" property.
 *
 * Side Effects:
 *	None.
 */
int
of_compatible(phandle, strings)
	int phandle;
	const char * const *strings;
{
	int len, allocated, rv;
	char *buf;
	const char *sp, *nsp;

	len = OF_getproplen(phandle, "compatible");
	if (len <= 0)
		return (-1);

	if (len > OFW_MAX_STACK_BUF_SIZE) {
		buf = malloc(len, M_TEMP, M_WAITOK);
		allocated = 1;
	} else {
		buf = alloca(len);
		allocated = 0;
	}

	/* 'compatible' size should not change. */
	if (OF_getprop(phandle, "compatible", buf, len) != len) {
		rv = -1;
		goto out;
	}

	sp = buf;
	while (len && (nsp = memchr(sp, 0, len)) != NULL) {
		/* look for a match among the strings provided */
		for (rv = 0; strings[rv] != NULL; rv++)
			if (strcmp(sp, strings[rv]) == 0)
				goto out;

		nsp++;			/* skip over NUL char */
		len -= (nsp - sp);
		sp = nsp;
	}
	rv = -1;

out:
	if (allocated)
		free(buf, M_TEMP);
	return (rv);
	
}

/*
 * int of_packagename(phandle, buf, bufsize)
 *
 * This routine places the last component of an OFW node's name
 * into a user-provided buffer.
 *
 * It can be used during autoconfiguration to make printing of
 * device names more informative.
 *
 * Arguments:
 *	phandle		OFW phandle of device whose name name is
 *			desired.
 *	buf		Buffer to contain device name, provided by
 *			caller.  (For now, must be at least 4
 *			bytes long.)
 *	bufsize		Length of buffer referenced by 'buf', in
 *			bytes.
 *
 * Return Value:
 *	-1 if the device path name could not be obtained or would
 *	not fit in the allocated temporary buffer, or zero otherwise
 *	(meaning that the leaf node name was successfully extracted).
 *
 * Side Effects:
 *	If the leaf node name was successfully extracted, 'buf' is
 *	filled in with at most 'bufsize' bytes of the leaf node
 *	name.  If the leaf node was not successfully extracted, a
 *	somewhat meaningful string is placed in the buffer.  In
 *	either case, the contents of 'buf' will be NUL-terminated.
 */
int
of_packagename(phandle, buf, bufsize)
	int phandle;
	char *buf;
	int bufsize;
{
	char *pbuf;
	const char *lastslash;
	int l, rv;

	pbuf = malloc(OFW_PATH_BUF_SIZE, M_TEMP, M_WAITOK);
	l = OF_package_to_path(phandle, pbuf, OFW_PATH_BUF_SIZE);

	/* check that we could get the name, and that it's not too long. */
	if (l < 0 ||
	    (l == OFW_PATH_BUF_SIZE && pbuf[OFW_PATH_BUF_SIZE - 1] != '\0')) {
		/* XXX should use snprintf! */
		if (bufsize >= 25)
			sprintf(buf, "??? (phandle 0x%x)", phandle);
		else if (bufsize >= 4)
			strcpy(buf, "???");
		else
			panic("of_packagename: bufsize = %d is silly",
			    bufsize);
		rv = -1;
	} else {
		pbuf[l] = '\0';
		lastslash = strrchr(pbuf, '/');
		strncpy(buf, (lastslash == NULL) ? pbuf : (lastslash + 1),
		    bufsize);
		buf[bufsize - 1] = '\0'; /* in case it's fills the buffer. */
		rv = 0;
	}

	free(pbuf, M_TEMP);
	return (rv);
}

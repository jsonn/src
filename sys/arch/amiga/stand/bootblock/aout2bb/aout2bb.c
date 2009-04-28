/*	$NetBSD: aout2bb.c,v 1.14.8.1 2009/04/28 07:33:41 skrll Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>		/* of the machine we're running on */
#include <sys/endian.h>		/* of the machine we're running on */

#define BE32TOH(x)	do {(x) = be32toh(x);} while (0)

#include <sys/exec_aout.h>	/* TARGET */

#include "aout2bb.h"
#include "chksum.h"

void usage(void);
int intcmp(const void *, const void *);
int main(int argc, char *argv[]);

#ifdef DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#define BBSIZE 8192

char *progname;
int bbsize = BBSIZE;
u_int8_t *buffer;
u_int32_t *relbuf;
	/* can't have more relocs than that*/

extern char *optarg;

int
intcmp(const void *i, const void *j)
{
	int r;

	r = (*(u_int32_t *)i) < (*(u_int32_t *)j);
	
	return 2*r-1;
}

int
main(int argc, char *argv[])
{
	int ifd, ofd;
	u_int mid, flags, magic;
	void *image;
	struct exec *eh;
	struct relocation_info_m68k *rpi;
	u_int32_t *lptr;
	int i, delta;
	u_int8_t *rpo;
	u_int32_t oldaddr, addrdiff;
	u_int32_t tsz, dsz, bsz, trsz, drsz, entry, relver;
	int sumsize = 16;
	int c;
	

	progname = argv[0];

	/* insert getopt here, if needed */
	while ((c = getopt(argc, argv, "FS:")) != -1)
	switch(c) {
	case 'F':
		sumsize = 2;
		break;
	case 'S':
		bbsize = (atoi(optarg) + 511) & ~511;
		break;
	default:
		usage();
	}
	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();

	buffer = malloc(bbsize);
	relbuf = (u_int32_t *)malloc(bbsize);
	if (buffer == NULL || relbuf == NULL)
		err(1, "Unable to allocate memory");

	ifd = open(argv[0], O_RDONLY, 0);
	if (ifd < 0)
		err(1, "Can't open %s", argv[0]);

/* XXX stat(ifd, sb), mmap(0, sb.st_size); */
	image = mmap(0, 65536, PROT_READ, MAP_FILE|MAP_PRIVATE, ifd, 0);
	if (image == 0)
		err(1, "Can't mmap %s", argv[1]);

	eh = (struct exec *)image; /* XXX endianness */

	magic = N_GETMAGIC(*eh);
	if (magic != OMAGIC)
		errx(1, "%s isn't an OMAGIC file, but 0%o", argv[0], magic);

	flags = N_GETFLAG(*eh);
	if (flags != 0)
		errx(1, "%s has strange exec flags 0x%x", argv[0], flags);

	mid = N_GETMID(*eh);
	switch(mid) {
	case MID_M68K:
	case MID_M68K4K:
		break;
	default:
		errx(1, "%s has strange machine id 0x%x (%d)", argv[0], mid,
		    mid);
	}

	tsz = ntohl(eh->a_text);
	dsz = ntohl(eh->a_data);
	bsz = ntohl(eh->a_bss);
	trsz = ntohl(eh->a_trsize);
	drsz = ntohl(eh->a_drsize);
	entry = ntohl(eh->a_entry);

	dprintf(("tsz = 0x%x, dsz = 0x%x, bsz = 0x%x, total 0x%x, entry=0x%x\n",
		tsz, dsz, bsz, tsz+dsz+bsz, entry));

	if ((trsz+drsz)==0)
		errx(1, "%s has no relocation records.", argv[0]);

	dprintf(("%d text relocs, %d data relocs\n", trsz/8, drsz/8));
	if (entry != 12)
		errx(1, "%s: entry point 0x%04x is not 0x000c", argv[0],
		    entry);

	/*
	 * We have one contiguous area allocated by the ROM to us.
	 */
	if (tsz+dsz+bsz > bbsize)
		errx(1, "%s: resulting image too big", argv[0]);

	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, image + N_TXTOFF(*eh), tsz+dsz);

	/*
	 * Hm. This tool REALLY should understand more than one
	 * relocator version. For now, check that the relocator at
	 * the image start does understand what we output.
	 */
	relver = ntohl(*(u_int32_t *)(image+0x24));
	switch (relver) {
		default:
			errx(1, "%s: unrecognized relocator version %d",
				argv[0], relver);
			/*NOTREACHED*/

		case RELVER_RELATIVE_BYTES:
			rpo = buffer + bbsize - 1;
			delta = -1;
			break;

		case RELVER_RELATIVE_BYTES_FORWARD:
			rpo = buffer + tsz + dsz;
			delta = +1;
			*(u_int16_t *)(buffer + 14) = htons(tsz + dsz);
			break;
	}


	
	i = 0;

	for (rpi = (struct relocation_info_m68k *)(image+N_TRELOFF(*eh));
	    (void *)rpi < image+N_TRELOFF(*eh)+trsz; rpi++) {

		BE32TOH(((u_int32_t *)rpi)[0]);
		BE32TOH(((u_int32_t *)rpi)[1]);

		dprintf(("0x%08x 0x%08x %c\n", *(u_int32_t *)rpi,
		    ((u_int32_t *)rpi)[1], rpi->r_extern ? 'U' : ' '));

		if (rpi->r_extern)
			errx(1, "code accesses unresolved symbol");
		if (rpi->r_copy)
			errx(1, "code accesses r_copy symbol");
		if (rpi->r_jmptable)
			errx(1, "code accesses r_jmptable symbol");
		if (rpi->r_relative)
			errx(1, "code accesses r_relative symbol");
		if (rpi->r_baserel)
			errx(1, "code accesses r_baserel symbol");

		/*
		 * We don't worry about odd sized symbols which are pc
		 * relative, so test for pcrel first:
		 */

		if (rpi->r_pcrel)
			continue;

		if (rpi->r_length != 2)
			errx(1, "code accesses size %d symbol", rpi->r_length);

		relbuf[i++] = rpi->r_address;
	}

	for (rpi = (struct relocation_info_m68k *)(image+N_DRELOFF(*eh));
	    (void *)rpi < image+N_DRELOFF(*eh)+drsz; rpi++) {

		BE32TOH(((u_int32_t *)rpi)[0]);
		BE32TOH(((u_int32_t *)rpi)[1]);

		dprintf(("0x%08x 0x%08x %c\n", *(u_int32_t *)rpi,
		    ((u_int32_t *)rpi)[1], rpi->r_extern ? 'U' : ' '));

		if (rpi->r_extern)
			errx(1, "data accesses unresolved symbol");
		if (rpi->r_copy)
			errx(1, "data accesses r_copy symbol");
		if (rpi->r_jmptable)
			errx(1, "data accesses r_jmptable symbol");
		if (rpi->r_relative)
			errx(1, "data accesses r_relative symbol");
		if (rpi->r_baserel)
			errx(1, "data accesses r_baserel symbol");

		/*
		 * We don't worry about odd sized symbols which are pc
		 * relative, so test for pcrel first:
		 */

		if (rpi->r_pcrel)
			continue;

		if (rpi->r_length != 2)
			errx(1, "data accesses size %d symbol", rpi->r_length);


		relbuf[i++] = rpi->r_address + tsz;
	}
	printf("%d absolute reloc%s found, ", i, i==1?"":"s");
	
	if (i > 1)
		heapsort(relbuf, i, 4, intcmp);

	oldaddr = 0;
	
	for (--i; i>=0; --i) {
		dprintf(("0x%04x: ", relbuf[i]));
		lptr = (u_int32_t *)&buffer[relbuf[i]];
		addrdiff = relbuf[i] - oldaddr;
		dprintf(("(0x%04x, 0x%04x): ", *lptr, addrdiff));
		if (addrdiff > 255) {
			*rpo = 0;
			if (delta > 0) {
				++rpo;
				*rpo++ = (relbuf[i] >> 8) & 0xff;
				*rpo++ = relbuf[i] & 0xff;
				dprintf(("%02x%02x%02x\n",
				    rpo[-3], rpo[-2], rpo[-1]));
			} else {
				*--rpo = relbuf[i] & 0xff;
				*--rpo = (relbuf[i] >> 8) & 0xff;
				--rpo;
				dprintf(("%02x%02x%02x\n",
				    rpo[0], rpo[1], rpo[2]));
			}
		} else {
			*rpo = addrdiff;
			dprintf(("%02x\n", *rpo));
			rpo += delta;
		}

		oldaddr = relbuf[i];

		if (delta < 0 ? rpo <= buffer+tsz+dsz
		    : rpo >= buffer + bbsize)
			errx(1, "Relocs don't fit.");
	}
	*rpo = 0; rpo += delta;
	*rpo = 0; rpo += delta;
	*rpo = 0; rpo += delta;

	printf("using %d bytes, %d bytes remaining.\n", delta > 0 ?
	    rpo-buffer-tsz-dsz : buffer+bbsize-rpo, delta > 0 ?
	    buffer + bbsize - rpo : rpo - buffer - tsz - dsz);
	/*
	 * RELOCs must fit into the bss area.
	 */
	if (delta < 0 ? rpo <= buffer+tsz+dsz
	    : rpo >= buffer + bbsize)
		errx(1, "Relocs don't fit.");

	((u_int32_t *)buffer)[1] = 0;
	((u_int32_t *)buffer)[1] =
	    (0xffffffff - chksum((u_int32_t *)buffer, sumsize * 512 / 4));

	ofd = open(argv[1], O_CREAT|O_WRONLY, 0644);
	if (ofd < 0)
		err(1, "Can't open %s", argv[1]);

	if (write(ofd, buffer, bbsize) != bbsize)
		err(1, "Writing output file");

	exit(0);
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-F] [-S bbsize] bootprog bootprog.bin\n",
	    progname);
	exit(1);
	/* NOTREACHED */
}

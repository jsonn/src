/*	$NetBSD: elf2bb.c,v 1.1.2.2 2002/01/10 19:37:27 thorpej Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>		/* of the machine we're running on */
#include <machine/endian.h>	/* of the machine we're running on */

#include <elf.h>		/* TARGET */
#ifndef R_68K_32		/* XXX host not m68k XXX */
#define	R_68K_32	1
#define	R_68K_PC32	4
#define	R_68K_PC16	5
#endif

#include "elf2bb.h"
#include "chksum.h"

void usage __P((void));
int intcmp(const void *, const void *);
int main(int argc, char *argv[]);

#ifdef DEBUG
#define dprintf(x) if (debug) printf x
#else
#define dprintf(x)
#endif
int debug;

#define BBSIZE 8192

char *progname;
int bbsize = BBSIZE;
u_int8_t *buffer;
u_int32_t *relbuf;
	/* can't have more relocs than that*/

int
intcmp(i, j)
	const void *i, *j;
{
	int r;

	r = (*(u_int32_t *)i) < (*(u_int32_t *)j);
	
	return 2*r-1; 
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ifd, ofd;
	u_int mid, flags, magic;
	caddr_t image;
	Elf32_Ehdr *eh;
	Elf32_Shdr *sh;
	char *shstrtab;
	Elf32_Sym *symtab;
	char *strtab;
	int eval(Elf32_Sym *, u_int32_t *);
	u_int32_t *lptr;
	int i, l, delta;
	u_int8_t *rpo;
	u_int32_t oldaddr, addrdiff;
	u_int32_t tsz, dsz, bsz, trsz, drsz, entry, relver;
	u_int32_t pcrelsz, r32sz;
	int sumsize = 16;
	int c;
	u_int32_t *sect_offset;
	

	progname = argv[0];

	/* insert getopt here, if needed */
	while ((c = getopt(argc, argv, "dFS:")) != -1)
	switch(c) {
	case 'F':
		sumsize = 2;
		break;
	case 'S':
		bbsize = (atoi(optarg) + 511) & ~511;
		sumsize = bbsize / 512;
		break;
	case 'd':
		debug = 1;
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
		err(1, "Unable to allocate memory\n");

	ifd = open(argv[0], O_RDONLY, 0);
	if (ifd < 0)
		err(1, "Can't open %s", argv[0]);

	image = mmap(0, 65536, PROT_READ, MAP_FILE|MAP_PRIVATE, ifd, 0);
	if (image == 0)
		err(1, "Can't mmap %s", argv[1]);

	eh = (Elf32_Ehdr *)image; /* XXX endianness */

	dprintf(("%04x sections, offset %08x\n", htobe16(eh->e_shnum), htobe32(eh->e_shoff)));
	if (htobe16(eh->e_type) != ET_REL)
		errx(1, "%s isn't a relocatable file, type=%d\n",
		    argv[0], htobe16(eh->e_type));
	if (htobe16(eh->e_machine) != EM_68K)
		errx(1, "%s isn't M68K, machine=%d\n", argv[0],
		    htobe16(eh->e_machine));

	tsz = dsz = bsz = trsz = pcrelsz = r32sz = 0;
	sh = (Elf32_Shdr *)(image + htobe32(eh->e_shoff));
	shstrtab = (char *)(image + htobe32(sh[htobe16(eh->e_shstrndx)].sh_offset));
	symtab = NULL;	/*XXX*/
	strtab = NULL;	/*XXX*/
	dprintf(("    name                      type     flags    addr     offset   size     align\n"));
	for (i = 0; i < htobe16(eh->e_shnum); ++i) {
		u_int32_t sh_size;

		dprintf( ("%2d: %08x %-16s %08x %08x %08x %08x %08x %08x\n", i,
		    htobe32(sh[i].sh_name), shstrtab + htobe32(sh[i].sh_name),
		    htobe32(sh[i].sh_type),
		    htobe32(sh[i].sh_flags), htobe32(sh[i].sh_addr),
		    htobe32(sh[i].sh_offset), htobe32(sh[i].sh_size),
		    htobe32(sh[i].sh_addralign)));
		sh_size = (htobe32(sh[i].sh_size) + htobe32(sh[i].sh_addralign) - 1) &
		    -htobe32(sh[i].sh_addralign);
		if (htobe32(sh[i].sh_type) == SHT_PROGBITS) {
			if (htobe32(sh[i].sh_flags) & SHF_WRITE)
				dsz += sh_size;
			else
				tsz += sh_size;
		} else if (htobe32(sh[i].sh_type) == SHT_NOBITS &&
		    htobe32(sh[i].sh_flags) == (SHF_ALLOC | SHF_WRITE)) {
			bsz += sh_size;;
		} else if (htobe32(sh[i].sh_type) == SHT_RELA) {
			trsz += htobe32(sh[i].sh_size);
		}
		/* Check for SHT_REL? */
		else if (htobe32(sh[i].sh_type) == SHT_SYMTAB) {
			symtab = (Elf32_Sym *)(image + htobe32(sh[i].sh_offset));
		} else if (strcmp(".strtab", shstrtab + htobe32(sh[i].sh_name)) == 0) {
			strtab = image + htobe32(sh[i].sh_offset);
		}
	}
	dprintf(("tsz = 0x%x, dsz = 0x%x, bsz = 0x%x, total 0x%x\n",
	    tsz, dsz, bsz, tsz + dsz + bsz));

	if (trsz == 0)
		errx(1, "%s has no relocation records.\n", argv[0]);

	dprintf(("%d relocs\n", trsz/12));

	/*
	 * We have one contiguous area allocated by the ROM to us.
	 */
	if (tsz+dsz+bsz > bbsize)
		errx(1, "%s: resulting image too big\n", argv[0]);

	memset(buffer, bbsize, 0);

	/* Allocate and load loadable sections */
	sect_offset = (u_int32_t *)malloc(htobe16(eh->e_shnum) * sizeof(u_int32_t));
	for (i = 0, l = 0; i < htobe16(eh->e_shnum); ++i) {
		if (htobe32(sh[i].sh_type) == SHT_PROGBITS ||
		    htobe32(sh[i].sh_type) == SHT_NOBITS) {
			dprintf(("vaddr 0x%04x size 0x%04x offset 0x%04x section %s\n",
			    l, htobe32(sh[i].sh_size), htobe32(sh[i].sh_offset),
			    shstrtab + htobe32(sh[i].sh_name)));
			if (htobe32(sh[i].sh_type) == SHT_PROGBITS)
				memcpy(buffer + l, image + htobe32(sh[i].sh_offset),
				    htobe32(sh[i].sh_size));
			sect_offset[i] = l;
			l += (htobe32(sh[i].sh_size) + htobe32(sh[i].sh_addralign) - 1) &
			    -htobe32(sh[i].sh_addralign);
		}
	}

	/*
	 * Hm. This tool REALLY should understand more than one 
	 * relocator version. For now, check that the relocator at
	 * the image start does understand what we output.
	 */
	relver = htobe32(*(u_int32_t *)(buffer + 4));
	switch (relver) {
	default:
		errx(1, "%s: unrecognized relocator version %d\n",
			argv[0], relver);
		/*NOTREACHED*/

	case RELVER_RELATIVE_BYTES:
		rpo = buffer + bbsize - 1;
		delta = -1;
		break;

	case RELVER_RELATIVE_BYTES_FORWARD:
		rpo = buffer + tsz + dsz;
		delta = +1;
		*(u_int16_t *)(buffer + 14) = htobe16(tsz + dsz);
		break;
	}

	if (symtab == NULL)
		errx(1, "No symbol table found\n");
	/*
	 * Link sections and generate relocation data
	 * Nasty:  .text, .rodata, .data, .bss sections are not linked
	 *         Symbol table values relative to start of sections.
	 * For each relocation entry:
	 *    Symbol value needs to be calculated: value + section offset
	 *    Image data adjusted to calculated value of symbol + addend
	 *    Add relocation table entry for 32-bit relocatable values
	 *    PC-relative entries will be absolute and don't need relocation
	 */
	for (i = 0; i < htobe16(eh->e_shnum); ++i) {
		int n;
		Elf32_Rela *ra;
		u_int8_t *base;

		if (htobe32(sh[i].sh_type) != SHT_RELA)
			continue;
		base = NULL;
		if (strncmp(shstrtab + htobe32(sh[i].sh_name), ".rela", 5) != 0)
			err(1, "bad relocation section name %s", shstrtab +
			    htobe32(sh[i].sh_name));
		for (n = 0; n < htobe16(eh->e_shnum); ++n) {
			if (strcmp(shstrtab + htobe32(sh[i].sh_name) + 5, shstrtab +
			    htobe32(sh[n].sh_name)) != 0)
				continue;
			base = buffer + sect_offset[n];
			break;
		}
		if (base == NULL)
			errx(1, "Can't find section for reloc %s\n", shstrtab +
			    htobe32(sh[i].sh_name));
		ra = (Elf32_Rela *)(image + htobe32(sh[i].sh_offset));
		for (n = 0; n < htobe32(sh[i].sh_size); n += sizeof(Elf32_Rela), ++ra) {
			int value;
			value = htobe32(ra->r_addend) +
			    eval(&symtab[ELF32_R_SYM(htobe32(ra->r_info))], sect_offset);
			dprintf(("reloc %04x info %04x (type %d sym %d) add 0x%x val %x\n",
			    htobe32(ra->r_offset), htobe32(ra->r_info),
			    ELF32_R_TYPE(htobe32(ra->r_info)),
			    ELF32_R_SYM(htobe32(ra->r_info)),
			    htobe32(ra->r_addend), value));
			switch (ELF32_R_TYPE(htobe32(ra->r_info))) {
			case R_68K_32:
				*((u_int32_t *)(base + htobe32(ra->r_offset))) =
				    htobe32(value);
				relbuf[r32sz++] = (base - buffer) + htobe32(ra->r_offset);
				break;
			case R_68K_PC32:
				++pcrelsz;
				*((int32_t *)(base + htobe32(ra->r_offset))) =
				    htobe32(value - htobe32(ra->r_offset));
				break;
			case R_68K_PC16:
				++pcrelsz;
				*((int16_t *)(base + htobe32(ra->r_offset))) =
				    htobe16(value - htobe32(ra->r_offset));
				break;
			default:
				errx(1, "Relocation type %d not supported",
				    ELF32_R_TYPE(htobe32(ra->r_info)));
			}
		}
	}
	dprintf(("%d PC-relative relocations, %d 32-bit relocations\n",
	    pcrelsz, r32sz));
	printf("%d absolute reloc%s found, ", r32sz, r32sz==1?"":"s");
	
	i = r32sz;
	if (i > 1)
		heapsort(relbuf, r32sz, 4, intcmp);

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
	    htobe32((0xffffffff - chksum((u_int32_t *)buffer, sumsize * 512 / 4)));

	ofd = open(argv[1], O_CREAT|O_WRONLY, 0644);
	if (ofd < 0)
		err(1, "Can't open %s", argv[1]);

	if (write(ofd, buffer, bbsize) != bbsize)
		err(1, "Writing output file");

	exit(0);
}

void
usage()
{
	fprintf(stderr, "Usage: %s [-F] bootprog bootprog.bin\n",
	    progname);
	exit(1);
	/* NOTREACHED */
}

int
eval(Elf32_Sym *s, u_int32_t *o)
{
	int value;

	value = htobe32(s->st_value);
	if (htobe16(s->st_shndx) < 0xf000)
		value += o[htobe16(s->st_shndx)];
	else
		printf("eval: %x\n", htobe16(s->st_shndx));
	return value;
}

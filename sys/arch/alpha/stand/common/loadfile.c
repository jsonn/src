/* $NetBSD: loadfile.c,v 1.5.2.1 1997/09/06 18:00:07 thorpej Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#define	ELFSIZE		64

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>
#include <sys/exec_elf.h>

#include <machine/prom.h>

#define _KERNEL
#include "include/pte.h"

#ifdef ALPHA_BOOT_ECOFF
static int coff_exec __P((int, struct ecoff_exechdr *, u_int64_t *));
#endif
#ifdef ALPHA_BOOT_ELF
static int elf_exec __P((int, Elf_Ehdr *, u_int64_t *));
#endif
int loadfile __P((char *, u_int64_t *));

vm_offset_t ffp_save, ptbr_save;
vm_offset_t ssym, esym;

/*
 * Open 'filename', read in program and return the entry point or -1 if error.
 */
int
loadfile(fname, entryp)
	char *fname;
	u_int64_t *entryp;
{
	union {
#ifdef ALPHA_BOOT_ECOFF
		struct ecoff_exechdr coff;
#endif
#ifdef ALPHA_BOOT_ELF
		Elf_Ehdr elf;
#endif
	} hdr;
	ssize_t nr;
	int fd, rval;

	printf("\nLoading %s...\n", fname);

	/* Open the file. */
	rval = 1;
	if ((fd = open(fname, 0)) < 0) {
		(void)printf("open %s: %s\n", fname, strerror(errno));
		goto err;
	}

	/* Read the exec header. */
	if ((nr = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
		(void)printf("read header: %s\n", strerror(errno));
		goto err;
	}

#ifdef ALPHA_BOOT_ECOFF
	if (!ECOFF_BADMAG(&hdr.coff)) {
		rval = coff_exec(fd, &hdr.coff, entryp);
	} else
#endif
#ifdef ALPHA_BOOT_ELF
	if (memcmp(Elf_e_ident, hdr.elf.e_ident, Elf_e_siz) == 0) {
		rval = elf_exec(fd, &hdr.elf, entryp);
	} else
#endif
	{
		(void)printf("%s: unknown executable format\n", fname);
	}

err:
	if (fd >= 0)
		(void)close(fd);
	return (rval);
}

#ifdef ALPHA_BOOT_ECOFF
static int
coff_exec(fd, coff, entryp)
	int fd;
	struct ecoff_exechdr *coff;
	u_int64_t *entryp;
{

	/* Read in text. */
	(void)printf("%lu", coff->a.tsize);
	(void)lseek(fd, ECOFF_TXTOFF(coff), 0);
	if (read(fd, (void *)coff->a.text_start, coff->a.tsize) !=
	    coff->a.tsize) {
		(void)printf("read text: %s\n", strerror(errno));
		return (1);
	}

	/* Read in data. */
	if (coff->a.dsize != 0) {
		(void)printf("+%lu", coff->a.dsize);
		if (read(fd, (void *)coff->a.data_start, coff->a.dsize) !=
		    coff->a.dsize) {
			(void)printf("read data: %s\n", strerror(errno));
			return (1);
		}
	}


	/* Zero out bss. */
	if (coff->a.bsize != 0) {
		(void)printf("+%lu", coff->a.bsize);
		bzero((void *)coff->a.bss_start, coff->a.bsize);
	}

	ffp_save = coff->a.text_start + coff->a.tsize;
	if (ffp_save < coff->a.data_start + coff->a.dsize)
		ffp_save = coff->a.data_start + coff->a.dsize;
	if (ffp_save < coff->a.bss_start + coff->a.bsize)
		ffp_save = coff->a.bss_start + coff->a.bsize;
	ffp_save = ALPHA_K0SEG_TO_PHYS((ffp_save + PGOFSET) & ~PGOFSET) >> PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	(void)printf("\n");
	*entryp = coff->a.entry;
	return (0);
}
#endif /* ALPHA_BOOT_ECOFF */

#ifdef ALPHA_BOOT_ELF
static int
elf_exec(fd, elf, entryp)
	int fd;
	Elf_Ehdr *elf;
	u_int64_t *entryp;
{
	Elf_Shdr *shp;
	Elf_Off off;
	int i;
	int first = 1;

	for (i = 0; i < elf->e_phnum; i++) {
		Elf_Phdr phdr;
		(void)lseek(fd, elf->e_phoff + sizeof(phdr) * i, 0);
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			(void)printf("read phdr: %s\n", strerror(errno));
			return (1);
		}
		if (phdr.p_type != Elf_pt_load ||
		    (phdr.p_flags & (Elf_pf_w|Elf_pf_x)) == 0)
			continue;

		/* Read in segment. */
		(void)printf("%s%lu", first ? "" : "+", phdr.p_filesz);
		(void)lseek(fd, phdr.p_offset, 0);
		if (read(fd, (void *)phdr.p_vaddr, phdr.p_filesz) !=
		    phdr.p_filesz) {
			(void)printf("read text: %s\n", strerror(errno));
			return (1);
		}
		if (first || ffp_save < phdr.p_vaddr + phdr.p_memsz)
			ffp_save = phdr.p_vaddr + phdr.p_memsz;

		/* Zero out bss. */
		if (phdr.p_filesz < phdr.p_memsz) {
			(void)printf("+%lu", phdr.p_memsz - phdr.p_filesz);
			bzero((void *)(phdr.p_vaddr + phdr.p_filesz),
			    phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}

	/*
	 * Copy the ELF and section headers.
	 */
	ssym = ffp_save;
	bcopy(elf, (void *)ffp_save, sizeof(Elf_Ehdr));
	ffp_save += sizeof(Elf_Ehdr);
	(void)lseek(fd, elf->e_shoff, SEEK_SET);
	if (read(fd, (void *)ffp_save, elf->e_shnum * sizeof(Elf_Shdr)) !=
	    elf->e_shnum * sizeof(Elf_Shdr)) {
		printf("read section headers: %s\n", strerror(errno));
		return (1);
	}
	shp = (Elf_Shdr *)ffp_save;
	ffp_save += roundup((elf->e_shnum * sizeof(Elf_Shdr)), sizeof(long));

	/*
	 * Now load the symbol sections themselves.  Make sure the
	 * sections are aligned.
	 */
	off = roundup((sizeof(Elf_Ehdr) + (elf->e_shnum * sizeof(Elf_Shdr))),
	    sizeof(long));
	for (first = 1, i = 0; i < elf->e_shnum; i++) {
		if (shp[i].sh_type == Elf_sht_symtab ||
		    shp[i].sh_type == Elf_sht_strtab) {
			printf("%s%ld", first ? " [" : "+", shp[i].sh_size);
			(void)lseek(fd, shp[i].sh_offset, SEEK_SET);
			if (read(fd, (void *)ffp_save, shp[i].sh_size) !=
			    shp[i].sh_size) {
				printf("\nread symbols: %s\n",
				    strerror(errno));
				return (1);
			}
			ffp_save += roundup(shp[i].sh_size, sizeof(long));
			shp[i].sh_offset = off;
			off += roundup(shp[i].sh_size, sizeof(long));
			first = 0;
		}
	}
	esym = ffp_save;

	if (first == 0)
		printf("]");

	ffp_save = ALPHA_K0SEG_TO_PHYS((ffp_save + PGOFSET) & ~PGOFSET)
	    >> PGSHIFT;
	ffp_save += 2;		/* XXX OSF/1 does this, no idea why. */

	(void)printf("\n");
	*entryp = elf->e_entry;

	/*
	 * Frob the copied ELF header to give information relative
	 * to ssym.
	 */
	elf = (Elf_Ehdr *)ssym;
	elf->e_phoff = 0;
	elf->e_shoff = sizeof(Elf_Ehdr);
	elf->e_phentsize = 0;
	elf->e_phnum = 0;

	return (0);
}
#endif /* ALPHA_BOOT_ELF */

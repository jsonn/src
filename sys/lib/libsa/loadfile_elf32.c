/* $NetBSD: loadfile_elf32.c,v 1.7.16.5 2005/11/10 14:10:24 skrll Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Christos Zoulas.
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

/* If not included by exec_elf64.c, ELFSIZE won't be defined. */
#ifndef ELFSIZE
#define	ELFSIZE	32
#endif

#ifdef _STANDALONE
#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#else
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#endif

#include <sys/param.h>
#include <sys/exec.h>

#include "loadfile.h"

#if ((ELFSIZE == 32) && defined(BOOT_ELF32)) || \
    ((ELFSIZE == 64) && defined(BOOT_ELF64))

#define	ELFROUND	(ELFSIZE / 8)

#ifndef _STANDALONE
#include "byteorder.h"

/*
 * Byte swapping may be necessary in the non-_STANDLONE case because
 * we may be built with a host compiler.
 */
#define	E16(f)								\
	f = (bo == ELFDATA2LSB) ? sa_htole16(f) : sa_htobe16(f)
#define	E32(f)								\
	f = (bo == ELFDATA2LSB) ? sa_htole32(f) : sa_htobe32(f)
#define	E64(f)								\
	f = (bo == ELFDATA2LSB) ? sa_htole64(f) : sa_htobe64(f)

#define	I16(f)								\
	f = (bo == ELFDATA2LSB) ? sa_le16toh(f) : sa_be16toh(f)
#define	I32(f)								\
	f = (bo == ELFDATA2LSB) ? sa_le32toh(f) : sa_be32toh(f)
#define	I64(f)								\
	f = (bo == ELFDATA2LSB) ? sa_le64toh(f) : sa_be64toh(f)

static void
internalize_ehdr(Elf_Byte bo, Elf_Ehdr *ehdr)
{

#if ELFSIZE == 32
	I16(ehdr->e_type);
	I16(ehdr->e_machine);
	I32(ehdr->e_version);
	I32(ehdr->e_entry);
	I32(ehdr->e_phoff);
	I32(ehdr->e_shoff);
	I32(ehdr->e_flags);
	I16(ehdr->e_ehsize);
	I16(ehdr->e_phentsize);
	I16(ehdr->e_phnum);
	I16(ehdr->e_shentsize);
	I16(ehdr->e_shnum);
	I16(ehdr->e_shstrndx);
#elif ELFSIZE == 64
	I16(ehdr->e_type);
	I16(ehdr->e_machine);
	I32(ehdr->e_version);
	I64(ehdr->e_entry);
	I64(ehdr->e_phoff);
	I64(ehdr->e_shoff);
	I32(ehdr->e_flags);
	I16(ehdr->e_ehsize);
	I16(ehdr->e_phentsize);
	I16(ehdr->e_phnum);
	I16(ehdr->e_shentsize);
	I16(ehdr->e_shnum);
	I16(ehdr->e_shstrndx);
#else
#error ELFSIZE is not 32 or 64
#endif
}

static void
externalize_ehdr(Elf_Byte bo, Elf_Ehdr *ehdr)
{

#if ELFSIZE == 32
	E16(ehdr->e_type);
	E16(ehdr->e_machine);
	E32(ehdr->e_version);
	E32(ehdr->e_entry);
	E32(ehdr->e_phoff);
	E32(ehdr->e_shoff);
	E32(ehdr->e_flags);
	E16(ehdr->e_ehsize);
	E16(ehdr->e_phentsize);
	E16(ehdr->e_phnum);
	E16(ehdr->e_shentsize);
	E16(ehdr->e_shnum);
	E16(ehdr->e_shstrndx);
#elif ELFSIZE == 64
	E16(ehdr->e_type);
	E16(ehdr->e_machine);
	E32(ehdr->e_version);
	E64(ehdr->e_entry);
	E64(ehdr->e_phoff);
	E64(ehdr->e_shoff);
	E32(ehdr->e_flags);
	E16(ehdr->e_ehsize);
	E16(ehdr->e_phentsize);
	E16(ehdr->e_phnum);
	E16(ehdr->e_shentsize);
	E16(ehdr->e_shnum);
	E16(ehdr->e_shstrndx);
#else
#error ELFSIZE is not 32 or 64
#endif
}

static void
internalize_phdr(Elf_Byte bo, Elf_Phdr *phdr)
{

#if ELFSIZE == 32
	I32(phdr->p_type);
	I32(phdr->p_offset);
	I32(phdr->p_vaddr);
	I32(phdr->p_paddr);
	I32(phdr->p_filesz);
	I32(phdr->p_memsz);
	I32(phdr->p_flags);
	I32(phdr->p_align);
#elif ELFSIZE == 64
	I32(phdr->p_type);
	I32(phdr->p_offset);
	I64(phdr->p_vaddr);
	I64(phdr->p_paddr);
	I64(phdr->p_filesz);
	I64(phdr->p_memsz);
	I64(phdr->p_flags);
	I64(phdr->p_align);
#else
#error ELFSIZE is not 32 or 64
#endif
}

static void
internalize_shdr(Elf_Byte bo, Elf_Shdr *shdr)
{

#if ELFSIZE == 32
	I32(shdr->sh_name);
	I32(shdr->sh_type);
	I32(shdr->sh_flags);
	I32(shdr->sh_addr);
	I32(shdr->sh_offset);
	I32(shdr->sh_size);
	I32(shdr->sh_link);
	I32(shdr->sh_info);
	I32(shdr->sh_addralign);
	I32(shdr->sh_entsize);
#elif ELFSIZE == 64
	I32(shdr->sh_name);
	I32(shdr->sh_type);
	I64(shdr->sh_flags);
	I64(shdr->sh_addr);
	I64(shdr->sh_offset);
	I64(shdr->sh_size);
	I32(shdr->sh_link);
	I32(shdr->sh_info);
	I64(shdr->sh_addralign);
	I64(shdr->sh_entsize);
#else
#error ELFSIZE is not 32 or 64
#endif
}

static void
externalize_shdr(Elf_Byte bo, Elf_Shdr *shdr)
{

#if ELFSIZE == 32
	E32(shdr->sh_name);
	E32(shdr->sh_type);
	E32(shdr->sh_flags);
	E32(shdr->sh_addr);
	E32(shdr->sh_offset);
	E32(shdr->sh_size);
	E32(shdr->sh_link);
	E32(shdr->sh_info);
	E32(shdr->sh_addralign);
	E32(shdr->sh_entsize);
#elif ELFSIZE == 64
	E32(shdr->sh_name);
	E32(shdr->sh_type);
	E64(shdr->sh_flags);
	E64(shdr->sh_addr);
	E64(shdr->sh_offset);
	E64(shdr->sh_size);
	E32(shdr->sh_link);
	E32(shdr->sh_info);
	E64(shdr->sh_addralign);
	E64(shdr->sh_entsize);
#else
#error ELFSIZE is not 32 or 64
#endif
}
#else /* _STANDALONE */
/*
 * Byte swapping is never necessary in the _STANDALONE case because
 * we are being built with the target compiler.
 */
#define	internalize_ehdr(bo, ehdr)	/* nothing */
#define	externalize_ehdr(bo, ehdr)	/* nothing */

#define	internalize_phdr(bo, phdr)	/* nothing */

#define	internalize_shdr(bo, shdr)	/* nothing */
#define	externalize_shdr(bo, shdr)	/* nothing */
#endif /* _STANDALONE */

int
ELFNAMEEND(loadfile)(fd, elf, marks, flags)
	int fd;
	Elf_Ehdr *elf;
	u_long *marks;
	int flags;
{
	Elf_Shdr *shp;
	Elf_Phdr *phdr;
	int i, j;
	ssize_t sz;
	int first;
	paddr_t minp = ~0, maxp = 0, pos = 0;
	paddr_t offset = marks[MARK_START], shpp, elfp = 0;

	/* some ports dont use the offset */
	offset = offset;

	internalize_ehdr(elf->e_ident[EI_DATA], elf);

	sz = elf->e_phnum * sizeof(Elf_Phdr);
	phdr = ALLOC(sz);

	if (lseek(fd, elf->e_phoff, SEEK_SET) == -1)  {
		WARN(("lseek phdr"));
		goto freephdr;
	}
	if (read(fd, phdr, sz) != sz) {
		WARN(("read program headers"));
		goto freephdr;
	}

	for (first = 1, i = 0; i < elf->e_phnum; i++) {
		internalize_phdr(elf->e_ident[EI_DATA], &phdr[i]);
		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_X)) == 0)
			continue;

#define IS_TEXT(p)	(p.p_flags & PF_X)
#define IS_DATA(p)	(p.p_flags & PF_W)
#define IS_BSS(p)	(p.p_filesz < p.p_memsz)
		/*
		 * XXX: Assume first address is lowest
		 */
		if ((IS_TEXT(phdr[i]) && (flags & LOAD_TEXT)) ||
		    (IS_DATA(phdr[i]) && (flags & LOAD_DATA))) {

			/* Read in segment. */
			PROGRESS(("%s%lu", first ? "" : "+",
			    (u_long)phdr[i].p_filesz));

			if (lseek(fd, phdr[i].p_offset, SEEK_SET) == -1)  {
				WARN(("lseek text"));
				goto freephdr;
			}
			if (READ(fd, phdr[i].p_vaddr, phdr[i].p_filesz) !=
			    (ssize_t)phdr[i].p_filesz) {
				WARN(("read text"));
				goto freephdr;
			}
			first = 0;

		}
		if ((IS_TEXT(phdr[i]) && (flags & (LOAD_TEXT|COUNT_TEXT))) ||
		    (IS_DATA(phdr[i]) && (flags & (LOAD_DATA|COUNT_TEXT)))) {
			pos = phdr[i].p_vaddr;
			if (minp > pos)
				minp = pos;
			pos += phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}

		/* Zero out bss. */
		if (IS_BSS(phdr[i]) && (flags & LOAD_BSS)) {
			PROGRESS(("+%lu",
			    (u_long)(phdr[i].p_memsz - phdr[i].p_filesz)));
			BZERO((phdr[i].p_vaddr + phdr[i].p_filesz),
			    phdr[i].p_memsz - phdr[i].p_filesz);
		}
		if (IS_BSS(phdr[i]) && (flags & (LOAD_BSS|COUNT_BSS))) {
			pos += phdr[i].p_memsz - phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}
	}
	FREE(phdr, sz);

	/*
	 * Copy the ELF and section headers.
	 */
	maxp = roundup(maxp, ELFROUND);
	if (flags & (LOAD_HDR|COUNT_HDR)) {
		elfp = maxp;
		maxp += sizeof(Elf_Ehdr);
	}

	if (flags & (LOAD_SYM|COUNT_SYM)) {
		if (lseek(fd, elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf_Shdr);

		shp = ALLOC(sz);

		if (read(fd, shp, sz) != sz) {
			WARN(("read section headers"));
			goto freeshp;
		}

		shpp = maxp;
		maxp += roundup(sz, ELFROUND);

#ifndef _STANDALONE
		/* Internalize the section headers. */
		for (i = 0; i < elf->e_shnum; i++)
			internalize_shdr(elf->e_ident[EI_DATA], &shp[i]);
#endif /* ! _STANDALONE */

		/*
		 * Now load the symbol sections themselves.  Make sure
		 * the sections are aligned. Don't bother with any
		 * string table that isn't referenced by a symbol
		 * table.
		 */
		for (first = 1, i = 0; i < elf->e_shnum; i++) {
			switch (shp[i].sh_type) {
			case SHT_STRTAB:
				for (j = 0; j < elf->e_shnum; j++)
					if (shp[j].sh_type == SHT_SYMTAB &&
					    shp[j].sh_link == (unsigned)i)
						goto havesym;
				/* FALLTHROUGH */
			default:
				/* Not loading this, so zero out the offset. */
				shp[i].sh_offset = 0;
				break;
			havesym:
			case SHT_SYMTAB:
				if (flags & LOAD_SYM) {
					PROGRESS(("%s%ld", first ? " [" : "+",
					    (u_long)shp[i].sh_size));
					if (lseek(fd, shp[i].sh_offset,
					    SEEK_SET) == -1) {
						WARN(("lseek symbols"));
						goto freeshp;
					}
					if (READ(fd, maxp, shp[i].sh_size) !=
					    (ssize_t)shp[i].sh_size) {
						WARN(("read symbols"));
						goto freeshp;
					}
				}
				shp[i].sh_offset = maxp - elfp;
				maxp += roundup(shp[i].sh_size, ELFROUND);
				first = 0;
			}
			/* Since we don't load .shstrtab, zero the name. */
			shp[i].sh_name = 0;
		}
		if (flags & LOAD_SYM) {
#ifndef _STANDALONE
			/* Externalize the section headers. */
			for (i = 0; i < elf->e_shnum; i++)
				externalize_shdr(elf->e_ident[EI_DATA],
				    &shp[i]);
#endif /* ! _STANDALONE */
			BCOPY(shp, shpp, sz);

			if (first == 0)
				PROGRESS(("]"));
		}
		FREE(shp, sz);
	}

	/*
	 * Frob the copied ELF header to give information relative
	 * to elfp.
	 */
	if (flags & LOAD_HDR) {
		elf->e_phoff = 0;
		elf->e_shoff = sizeof(Elf_Ehdr);
		elf->e_phentsize = 0;
		elf->e_phnum = 0;
		elf->e_shstrndx = SHN_UNDEF;
		externalize_ehdr(elf->e_ident[EI_DATA], elf);
		BCOPY(elf, elfp, sizeof(*elf));
		internalize_ehdr(elf->e_ident[EI_DATA], elf);
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(elf->e_entry);
	/*
	 * Since there can be more than one symbol section in the code
	 * and we need to find strtab too in order to do anything
	 * useful with the symbols, we just pass the whole elf
	 * header back and we let the kernel debugger find the
	 * location and number of symbols by itself.
	 */
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(elfp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;
freephdr:
	FREE(phdr, sz);
	return 1;
freeshp:
	FREE(shp, sz);
	return 1;
}

#endif /* (ELFSIZE == 32 && BOOT_ELF32) || (ELFSIZE == 64 && BOOT_ELF64) */

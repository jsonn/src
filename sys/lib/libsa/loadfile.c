/* $NetBSD: loadfile.c,v 1.10.4.1 2001/08/03 04:13:46 lukem Exp $ */

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

#ifdef BOOT_ECOFF
#include <sys/exec_ecoff.h>
static int coff_exec __P((int, struct ecoff_exechdr *, u_long *, int));
#endif
#ifdef BOOT_ELF
#include <sys/exec_elf.h>
static int elf_exec __P((int, Elf_Ehdr *, u_long *, int));
#endif
#ifdef BOOT_AOUT
#include <sys/exec_aout.h>
static int aout_exec __P((int, struct exec *, u_long *, int));
#endif

/*
 * Open 'filename', read in program and and return 0 if ok 1 on error.
 * Fill in marks
 */
int
loadfile(fname, marks, flags)
	const char *fname;
	u_long *marks;
	int flags;
{
	union {
#ifdef BOOT_ECOFF
		struct ecoff_exechdr coff;
#endif
#ifdef BOOT_ELF
		Elf_Ehdr elf;
#endif
#ifdef BOOT_AOUT
		struct exec aout;
#endif
		
	} hdr;
	ssize_t nr;
	int fd, rval;

	/* Open the file. */
	if ((fd = open(fname, 0)) < 0) {
		WARN(("open %s", fname ? fname : "<default>"));
		return -1;
	}

	/* Read the exec header. */
	if ((nr = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
		WARN(("read header"));
		goto err;
	}

#ifdef BOOT_ECOFF
	if (!ECOFF_BADMAG(&hdr.coff)) {
		rval = coff_exec(fd, &hdr.coff, marks, flags);
	} else
#endif
#ifdef BOOT_ELF
	if (memcmp(hdr.elf.e_ident, ELFMAG, SELFMAG) == 0 &&
	    hdr.elf.e_ident[EI_CLASS] == ELFCLASS) {
		rval = elf_exec(fd, &hdr.elf, marks, flags);
	} else
#endif
#ifdef BOOT_AOUT
	if (OKMAGIC(N_GETMAGIC(hdr.aout))
#ifndef NO_MID_CHECK
	    && N_GETMID(hdr.aout) == MID_MACHINE
#endif
	    ) {
		rval = aout_exec(fd, &hdr.aout, marks, flags);
	} else
#endif
	{
		rval = 1;
		errno = EFTYPE;
		WARN(("%s", fname ? fname : "<default>"));
	}

	if (rval == 0) {
		PROGRESS(("=0x%lx\n", marks[MARK_END] - marks[MARK_START]));
		return fd;
	}
err:
	(void)close(fd);
	return -1;
}

#ifdef BOOT_ECOFF
static int
coff_exec(fd, coff, marks, flags)
	int fd;
	struct ecoff_exechdr *coff;
	u_long *marks;
	int flags;
{
	paddr_t offset = marks[MARK_START];
	paddr_t minp = ~0, maxp = 0, pos;

	/* Read in text. */
	if (lseek(fd, ECOFF_TXTOFF(coff), SEEK_SET) == -1)  {
		WARN(("lseek text"));
		return 1;
	}

	if (coff->a.tsize != 0) {
		if (flags & LOAD_TEXT) {
			PROGRESS(("%lu", coff->a.tsize));
			if (READ(fd, coff->a.text_start, coff->a.tsize) !=
			    coff->a.tsize) {
				return 1;
			}
		}
		else {
			if (lseek(fd, coff->a.tsize, SEEK_CUR) == -1) {
				WARN(("read text"));
				return 1;
			}
		}
		if (flags & (COUNT_TEXT|LOAD_TEXT)) {
			pos = coff->a.text_start;
			if (minp > pos)
				minp = pos;
			pos += coff->a.tsize;
			if (maxp < pos)
				maxp = pos;
		}
	}

	/* Read in data. */
	if (coff->a.dsize != 0) {
		if (flags & LOAD_DATA) {
			PROGRESS(("+%lu", coff->a.dsize));
			if (READ(fd, coff->a.data_start, coff->a.dsize) !=
			    coff->a.dsize) {
				WARN(("read data"));
				return 1;
			}
		}
		if (flags & (COUNT_DATA|LOAD_DATA)) {
			pos = coff->a.data_start;
			if (minp > pos)
				minp = pos;
			pos += coff->a.dsize;
			if (maxp < pos)
				maxp = pos;
		}
	}

	/* Zero out bss. */
	if (coff->a.bsize != 0) {
		if (flags & LOAD_BSS) {
			PROGRESS(("+%lu", coff->a.bsize));
			BZERO(coff->a.bss_start, coff->a.bsize);
		}
		if (flags & (COUNT_BSS|LOAD_BSS)) {
			pos = coff->a.bss_start;
			if (minp > pos)
				minp = pos;
			pos = coff->a.bsize;
			if (maxp < pos)
				maxp = pos;
		}
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(coff->a.entry);
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(maxp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;
}
#endif /* BOOT_ECOFF */

#ifdef BOOT_ELF
static int
elf_exec(fd, elf, marks, flags)
	int fd;
	Elf_Ehdr *elf;
	u_long *marks;
	int flags;
{
	Elf_Shdr *shp;
	int i, j;
	size_t sz;
	int first;
	paddr_t minp = ~0, maxp = 0, pos = 0;
	paddr_t offset = marks[MARK_START], shpp, elfp = NULL;

	for (first = 1, i = 0; i < elf->e_phnum; i++) {
		Elf_Phdr phdr;
		if (lseek(fd, elf->e_phoff + sizeof(phdr) * i, SEEK_SET)
		    == -1)  {
			WARN(("lseek phdr"));
			return 1;
		}
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			WARN(("read phdr"));
			return 1;
		}
		if (phdr.p_type != PT_LOAD ||
		    (phdr.p_flags & (PF_W|PF_X)) == 0)
			continue;

#define IS_TEXT(p)	(p.p_flags & PF_X)
#define IS_DATA(p)	(p.p_flags & PF_W)
#define IS_BSS(p)	(p.p_filesz < p.p_memsz)
		/*
		 * XXX: Assume first address is lowest
		 */
		if ((IS_TEXT(phdr) && (flags & LOAD_TEXT)) ||
		    (IS_DATA(phdr) && (flags & LOAD_DATA))) {

			/* Read in segment. */
			PROGRESS(("%s%lu", first ? "" : "+",
			    (u_long)phdr.p_filesz));

			if (lseek(fd, phdr.p_offset, SEEK_SET) == -1)  {
				WARN(("lseek text"));
				return 1;
			}
			if (READ(fd, phdr.p_vaddr, phdr.p_filesz) !=
			    phdr.p_filesz) {
				WARN(("read text"));
				return 1;
			}
			first = 0;

		}
		if ((IS_TEXT(phdr) && (flags & (LOAD_TEXT|COUNT_TEXT))) ||
		    (IS_DATA(phdr) && (flags & (LOAD_DATA|COUNT_TEXT)))) {
			pos = phdr.p_vaddr;
			if (minp > pos)
				minp = pos;
			pos += phdr.p_filesz;
			if (maxp < pos)
				maxp = pos;
		}

		/* Zero out bss. */
		if (IS_BSS(phdr) && (flags & LOAD_BSS)) {
			PROGRESS(("+%lu",
			    (u_long)(phdr.p_memsz - phdr.p_filesz)));
			BZERO((phdr.p_vaddr + phdr.p_filesz),
			    phdr.p_memsz - phdr.p_filesz);
		}
		if (IS_BSS(phdr) && (flags & (LOAD_BSS|COUNT_BSS))) {
			pos += phdr.p_memsz - phdr.p_filesz;
			if (maxp < pos)
				maxp = pos;
		}
	}

	/*
	 * Copy the ELF and section headers.
	 */
	maxp = roundup(maxp, sizeof(long));
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
			return 1;
		}

		shpp = maxp;
		maxp += roundup(sz, sizeof(long));

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
					    shp[j].sh_link == i)
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
						FREE(shp, sz);
						return 1;
					}
					if (READ(fd, maxp, shp[i].sh_size) !=
					    shp[i].sh_size) {
						WARN(("read symbols"));
						FREE(shp, sz);
						return 1;
					}
				}
				shp[i].sh_offset = maxp - elfp;
				maxp += roundup(shp[i].sh_size,
				    sizeof(long));
				first = 0;
			}
			/* Since we don't load .shstrtab, zero the name. */
			shp[i].sh_name = 0;
		}
		if (flags & LOAD_SYM) {
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
		BCOPY(elf, elfp, sizeof(*elf));
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
}
#endif /* BOOT_ELF */

#ifdef BOOT_AOUT
static int
aout_exec(fd, x, marks, flags)
	int fd;
	struct exec *x;
	u_long *marks;
	int flags;
{
	u_long entry = x->a_entry;
	paddr_t aoutp = 0;
	paddr_t minp, maxp;
	int cc;
	paddr_t offset = marks[MARK_START];
	u_long magic = N_GETMAGIC(*x);
	int sub;

	/* In OMAGIC and NMAGIC, exec header isn't part of text segment */
	if (magic == OMAGIC || magic == NMAGIC)
		sub = 0;
	else
		sub = sizeof(*x);
	
	minp = maxp = ALIGNENTRY(entry);

	if (lseek(fd, sizeof(*x), SEEK_SET) == -1)  {
		WARN(("lseek text"));
		return 1;
	}

	/*
	 * Leave a copy of the exec header before the text.
	 * The kernel may use this to verify that the
	 * symbols were loaded by this boot program.
	 */
	if (magic == OMAGIC || magic == NMAGIC) {
		if (flags & LOAD_HDR && maxp >= sizeof(*x))
			BCOPY(x, maxp - sizeof(*x), sizeof(*x));
	}
	else {
		if (flags & LOAD_HDR)
			BCOPY(x, maxp, sizeof(*x));
		if (flags & (LOAD_HDR|COUNT_HDR))
			maxp += sizeof(*x);
	}

	/*
	 * Read in the text segment.
	 */
	if (flags & LOAD_TEXT) {
		PROGRESS(("%ld", x->a_text));

		if (READ(fd, maxp, x->a_text - sub) != x->a_text - sub) {
			WARN(("read text"));
			return 1;
		}
	} else {
		if (lseek(fd, x->a_text - sub, SEEK_CUR) == -1) {
			WARN(("seek text"));
			return 1;
		}
	}
	if (flags & (LOAD_TEXT|COUNT_TEXT))
		maxp += x->a_text - sub;

	/*
	 * Provide alignment if required
	 */
	if (magic == ZMAGIC || magic == NMAGIC) {
		int size = -(unsigned int)maxp & (__LDPGSZ - 1);

		if (flags & LOAD_TEXTA) {
			PROGRESS(("/%d", size));
			BZERO(maxp, size);
		}

		if (flags & (LOAD_TEXTA|COUNT_TEXTA))
			maxp += size;
	}

	/*
	 * Read in the data segment.
	 */
	if (flags & LOAD_DATA) {
		PROGRESS(("+%ld", x->a_data));

		if (READ(fd, maxp, x->a_data) != x->a_data) {
			WARN(("read data"));
			return 1;
		}
	}
	else {
		if (lseek(fd, x->a_data, SEEK_CUR) == -1) {
			WARN(("seek data"));
			return 1;
		}
	}
	if (flags & (LOAD_DATA|COUNT_DATA))
		maxp += x->a_data;

	/*
	 * Zero out the BSS section.
	 * (Kernel doesn't care, but do it anyway.)
	 */
	if (flags & LOAD_BSS) {
		PROGRESS(("+%ld", x->a_bss));

		BZERO(maxp, x->a_bss);
	}

	if (flags & (LOAD_BSS|COUNT_BSS))
		maxp += x->a_bss;

	/*
	 * Read in the symbol table and strings.
	 * (Always set the symtab size word.)
	 */
	if (flags & LOAD_SYM)
		BCOPY(&x->a_syms, maxp, sizeof(x->a_syms));

	if (flags & (LOAD_SYM|COUNT_SYM)) {
		maxp += sizeof(x->a_syms);
		aoutp = maxp;
	}

	if (x->a_syms > 0) {
		/* Symbol table and string table length word. */

		if (flags & LOAD_SYM) {
			PROGRESS(("+[%ld", x->a_syms));

			if (READ(fd, maxp, x->a_syms) != x->a_syms) {
				WARN(("read symbols"));
				return 1;
			}
		} else  {
			if (lseek(fd, x->a_syms, SEEK_CUR) == -1) {
				WARN(("seek symbols"));
				return 1;
			}
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += x->a_syms;

		if (read(fd, &cc, sizeof(cc)) != sizeof(cc)) {
			WARN(("read string table"));
			return 1;
		}

		if (flags & LOAD_SYM) {
			BCOPY(&cc, maxp, sizeof(cc));

			/* String table. Length word includes itself. */

			PROGRESS(("+%d]", cc));
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += sizeof(cc);

		cc -= sizeof(int);
		if (cc <= 0) {
			WARN(("symbol table too short"));
			return 1;
		}

		if (flags & LOAD_SYM) {
			if (READ(fd, maxp, cc) != cc) {
				WARN(("read strings"));
				return 1;
			}
		} else {
			if (lseek(fd, cc, SEEK_CUR) == -1) {
				WARN(("seek strings"));
				return 1;
			}
		}
		if (flags & (LOAD_SYM|COUNT_SYM))
			maxp += cc;
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(entry);
	marks[MARK_NSYM] = x->a_syms;
	marks[MARK_SYM] = LOADADDR(aoutp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;
}
#endif /* BOOT_AOUT */

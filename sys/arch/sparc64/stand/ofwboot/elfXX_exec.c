/*	$NetBSD: elfXX_exec.c,v 1.3.14.1 1999/11/15 00:39:39 fvdl Exp $	*/

/*
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * ELF support derived from NetBSD/alpha's boot loader, written
 * by Christopher G. Demetriou.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * First try for the boot code
 *
 * Input syntax is:
 *	[promdev[{:|,}partition]]/[filename] [flags]
 */

#define CONCAT(x,y)	__CONCAT(x,y)
#define CAT3(s,m,e)	CONCAT(s,CONCAT(m,e))

#if 0
int	CAT3(elf,ELFSIZE,_exec) __P((int, CAT3(Elf,ELFSIZE,_Ehdr) *, u_int64_t *, void **, void **));
#endif

int
CAT3(elf, ELFSIZE, _exec)(fd, elf, entryp, ssymp, esymp)
	int fd;
	CAT3(Elf,ELFSIZE,_Ehdr) *elf;
	u_int64_t *entryp;
	void **ssymp;
	void **esymp;
{
	CAT3(Elf,ELFSIZE,_Shdr) *shp;
	CAT3(Elf,ELFSIZE,_Off) off;
	void *addr;
	size_t size;
	int i, first = 1;
	int n;

	/*
	 * Don't display load address for ELF; it's encoded in
	 * each section.
	 */
#ifdef DEBUG
	printf("elf%d_exec: ", ELFSIZE);
#endif
	printf("Booting %s\n", opened_name);
printf("reading %ld program headers\n", (long)elf->e_phnum);

	for (i = 0; i < elf->e_phnum; i++) {
		CAT3(Elf,ELFSIZE,_Phdr) phdr;
printf("reading phdr %d at %lx\n", i, (long)(elf->e_phoff + sizeof(phdr) * i));
		size = lseek(fd, (size_t)(elf->e_phoff + sizeof(phdr) * i), SEEK_SET);
printf("lseek sez: %lx %s\n", (long)size, (size<0)?strerror(errno):"");
		if (read(fd, (void *)&phdr, sizeof(phdr)) != sizeof(phdr)) {
			printf("read phdr: %s\n", strerror(errno));
			return (1);
		}
printf("reading phdr worked, type %lx flags %lx\n", (long)phdr.p_type, (long)phdr.p_flags);
		if (phdr.p_type != PT_LOAD ||
		    (phdr.p_flags & (PF_W|PF_X)) == 0)
			continue;

		/* Read in segment. */
		printf("%s%lu@0x%lx", first ? "" : "+", (u_long)phdr.p_filesz,
		    (u_long)phdr.p_vaddr);
		(void)lseek(fd, (size_t)phdr.p_offset, SEEK_SET);
/* NB need to do 4MB allocs here */
		if (OF_claim((void *)(long)phdr.p_vaddr, phdr.p_memsz, phdr.p_align) ==
		    (void *)-1)
			panic("cannot claim memory");
		if (read(fd, (void *)(long)phdr.p_vaddr, phdr.p_filesz) !=
		    phdr.p_filesz) {
			printf("read segment: %s\n", strerror(errno));
			return (1);
		}
		syncicache((void *)(long)phdr.p_vaddr, phdr.p_filesz);

		/* Zero BSS. */
		if (phdr.p_filesz < phdr.p_memsz) {
			printf("+%lu@0x%lx", (u_long)phdr.p_memsz - phdr.p_filesz,
			    (u_long)(phdr.p_vaddr + phdr.p_filesz));
			bzero((void*)(long)phdr.p_vaddr + phdr.p_filesz,
			    (size_t)phdr.p_memsz - phdr.p_filesz);
		}
		first = 0;
	}

	printf(" \n");

	/*
	 * Compute the size of the symbol table.
	 */
	size = sizeof(CAT3(Elf,ELFSIZE,_Ehdr)) + (elf->e_shnum * sizeof(CAT3(Elf,ELFSIZE,_Shdr)));
	shp = addr = alloc(elf->e_shnum * sizeof(CAT3(Elf,ELFSIZE,_Shdr)));
	(void)lseek(fd, (off_t)elf->e_shoff, SEEK_SET);
	if (read(fd, addr, (size_t)(elf->e_shnum * sizeof(CAT3(Elf,ELFSIZE,_Shdr)))) !=
	    elf->e_shnum * sizeof(CAT3(Elf,ELFSIZE,_Shdr))) {
		printf("read section headers: %s\n", strerror(errno));
		return (1);
	}
	for (i = 0; i < elf->e_shnum; i++, shp++) {
		if (shp->sh_type == SHT_NULL)
			continue;
		if (shp->sh_type != SHT_SYMTAB
		    && shp->sh_type != SHT_STRTAB) {
			shp->sh_offset = 0; 
			shp->sh_type = SHT_NOBITS;
			continue;
		}
		size += shp->sh_size;
	}
	shp = addr;

	/*
	 * Reserve memory for the symbols.
	 */
	if ((addr = OF_claim(0, size, NBPG)) == (void *)-1)
		panic("no space for symbol table");

	/*
	 * Copy the headers.
	 */
	elf->e_phoff = 0;
	elf->e_shoff = sizeof(CAT3(Elf,ELFSIZE,_Ehdr));
	elf->e_phentsize = 0;
	elf->e_phnum = 0;
	bcopy(elf, addr, sizeof(CAT3(Elf,ELFSIZE,_Ehdr)));
	bcopy(shp, addr + sizeof(CAT3(Elf,ELFSIZE,_Ehdr)), 
	      elf->e_shnum * sizeof(CAT3(Elf,ELFSIZE,_Shdr)));
	free(shp, elf->e_shnum * sizeof(CAT3(Elf,ELFSIZE,_Shdr)));
	*ssymp = addr;

	/*
	 * Now load the symbol sections themselves.
	 */
	shp = addr + sizeof(CAT3(Elf,ELFSIZE,_Ehdr));
	addr += sizeof(CAT3(Elf,ELFSIZE,_Ehdr)) + (elf->e_shnum * sizeof(CAT3(Elf,ELFSIZE,_Shdr)));
	off = sizeof(CAT3(Elf,ELFSIZE,_Ehdr)) + (elf->e_shnum * sizeof(CAT3(Elf,ELFSIZE,_Shdr)));
	for (first = 1, i = 0; i < elf->e_shnum; i++, shp++) {
		if (shp->sh_type == SHT_SYMTAB
		    || shp->sh_type == SHT_STRTAB) {
			if (first)
				printf("symbols @ 0x%lx ", (u_long)addr);
			printf("%s%d", first ? "" : "+", (int)shp->sh_size);
			(void)lseek(fd, shp->sh_offset, SEEK_SET);
			if (read(fd, addr, shp->sh_size) != shp->sh_size) {
				printf("read symbols: %s\n", strerror(errno));
				return (1);
			}
			addr += (shp->sh_size+3)&(~3);
			shp->sh_offset = off;
			off += (shp->sh_size+3)&(~3);
			first = 0;
		}
	}
	*esymp = addr;

	*entryp = elf->e_entry;
	return (0);
}

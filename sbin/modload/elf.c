/*	$NetBSD: elf.c,v 1.2.2.1 1999/12/27 18:30:26 wrstuden Exp $	*/

/*
 * Copyright (c) 1998 Johan Danielsson <joda@pdc.kth.se> 
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: elf.c,v 1.2.2.1 1999/12/27 18:30:26 wrstuden Exp $");

#include <sys/param.h>

#if defined(__alpha__) || defined(__arch64__)
#define ELFSIZE 64
#else
#define ELFSIZE 32
#endif
#include <sys/exec_elf.h>
#ifndef ELF_HDR_SIZE
#define ELF_HDR_SIZE sizeof(Elf_Ehdr)
#endif
#include <sys/lkm.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "modload.h"

static void
read_section_header(int fd, Elf_Ehdr *ehdr, int num, Elf_Shdr *shdr)
{

	if (lseek(fd, ehdr->e_shoff + num * ehdr->e_shentsize, SEEK_SET) < 0)
		err(1, "lseek");
	if (read(fd, shdr, sizeof(*shdr)) != sizeof(*shdr))
		err(1, "read");
}

struct elf_section {
	char *name;		/* name of section; points into string table */
	unsigned long type;	/* type of section */
	void *addr;		/* load address of section */
	off_t offset;		/* offset in file */
	size_t size;		/* size of section */
	size_t align;
	struct elf_section *next;
};

/* adds the section `s' at the correct (sorted by address) place in
   the list ponted to by head; *head may be NULL */
static void
add_section(struct elf_section **head, struct elf_section *s)
{
	struct elf_section *p, **q;
	q = head;
	p = *head;

	while (1) {
		if (p == NULL || p->addr > s->addr) {
			s->next = p;
			*q = s;
			return;
		}
		q = &p->next;
		p = p->next;
	}
}

/* make a linked list of all sections containing ALLOCatable data */
static void
read_sections(int fd, Elf_Ehdr *ehdr, char *strtab, struct elf_section **head)
{
	int i;
	Elf_Shdr shdr;
	*head = NULL;
	/* scan through section headers */
	for (i = 0; i < ehdr->e_shnum; i++) {
		struct elf_section *s;
		read_section_header(fd, ehdr, i, &shdr);
		if ((shdr.sh_flags & SHF_ALLOC) == 0)
			/* skip non-ALLOC sections */
			continue;
		s = malloc(sizeof(*s));
		if (s == NULL)
			errx(1, "failed to allocate %lu bytes", (u_long)sizeof(*s));
		s->name = strtab + shdr.sh_name;
		s->type = shdr.sh_type;
		s->addr = (void*)shdr.sh_addr;
		s->offset = shdr.sh_offset;
		s->size = shdr.sh_size;
		s->align = shdr.sh_addralign;
		add_section(head, s);
	}
}

/* free a list of section headers */
static void
free_sections(struct elf_section *head)
{

	while (head) {
		struct elf_section *p = head;
		head = head->next;
		free(p);
	}
}

/* read string table */
static char *
read_string_table(int fd, Elf_Ehdr *ehdr)
{
	Elf_Shdr shdr;
	char *strtab;
	read_section_header(fd, ehdr, ehdr->e_shstrndx, &shdr);
	strtab = malloc(shdr.sh_size);
	if (strtab == NULL)
		errx(1, "failed to allocate %lu bytes", (u_long)shdr.sh_size);
	if (lseek(fd, shdr.sh_offset, SEEK_SET) < 0)
		err(1, "lseek");
	if (read(fd, strtab, shdr.sh_size) != shdr.sh_size)
		err(1, "read");
	return strtab;
}

static int
read_elf_header(int fd, Elf_Ehdr *ehdr)
{
	ssize_t n;

	n = read(fd, ehdr, sizeof(*ehdr));
	if (n < 0)
		err(1, "failed reading %lu bytes", (u_long)sizeof(*ehdr));
	if (n != sizeof(*ehdr)) {
		if (debug)
			warnx("failed to read %lu bytes", (u_long)sizeof(*ehdr));
		return -1;
	}
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0 ||
	    ehdr->e_ident[EI_CLASS] != ELFCLASS)
		errx(4, "not in ELF%u format", ELFSIZE);
	if (ehdr->e_ehsize != ELF_HDR_SIZE)
		errx(4, "file has ELF%u identity, but wrong header size", 
		    ELFSIZE);

	return 0;
}

/* offset of data segment; this is horrible, but keeps the size of the
   module to a minimum */
static ssize_t data_offset;

/* return size needed by the module */
int
elf_mod_sizes(fd, modsize, strtablen, resrvp, sp)
	int fd;
	size_t *modsize;
	int *strtablen;
	struct lmc_resrv *resrvp;
	struct stat *sp;
{
	Elf_Ehdr ehdr;
	ssize_t off = 0;
	size_t data_hole = 0;
	char *strtab;
	struct elf_section *head, *s;
	
	if (read_elf_header(fd, &ehdr) < 0)
		return -1;
	strtab = read_string_table(fd, &ehdr);
	read_sections(fd, &ehdr, strtab, &head);

	for (s = head; s; s = s->next) {
		if (debug)
			fprintf(stderr, 
			    "%s: addr = %p size = %#lx align = %#lx\n", 
			    s->name, s->addr, (u_long)s->size, (u_long)s->align);
		/* XXX try to get rid of the hole before the data
                   section that GNU-ld likes to put there */
		if (strcmp(s->name, ".data") == 0 && s->addr > (void*)off) {
			if (debug)
				fprintf(stderr, ".data section forced to "
				    "offset %p (was %p)\n", 
				    (void*)off, 
				    s->addr);
			data_offset = off;
			/* later remove size of compressed hole from off */
			data_hole = (ssize_t)s->addr - off;
		}
		off = (ssize_t)s->addr + s->size;
	}
	off -= data_hole;
	
	/* cleanup */
	free_sections(head);
	free(strtab);

	/* XXX round to pagesize? */
#define ROUND(V, S) (((V) + (S) - 1) & ~((S) - 1))
	*modsize = ROUND(off, sysconf(_SC_PAGESIZE));

	/*
	 * XXX no symbol table support
	 */
	*strtablen = 0;
	resrvp->sym_size = resrvp->sym_symsize = 0;

	return (0);
}

/*
 * Expected linker options:
 *
 * -R		executable to link against
 * -e		entry point
 * -o		output file
 * -Ttext	address to link text segment to in hex (assumes it's 
 *		a page boundry)
 * -Tdata	address to link data segment to in hex
 * <target>	object file */

#define	LINKCMD		"ld -R %s -e %s -o %s -Ttext %p %s"
#define	LINKCMD2	"ld -R %s -e %s -o %s -Ttext %p -Tdata %p %s"

/* make a link command; XXX if data_offset above is non-zero, force
   data address to be at start of text + offset */
void
elf_linkcmd(char *buf,
	    size_t len,
	    const char *kernel, 
	    const char *entry, 
	    const char *outfile, 
	    const void *address, 
	    const char *object)
{
	ssize_t n;

	if (data_offset == NULL)
		n = snprintf(buf, len, LINKCMD, kernel, entry, 
			     outfile, address, object);
	else
		n = snprintf(buf, len, LINKCMD2, kernel, entry, 
			     outfile, address, 
			     (const char*)address + data_offset, object);
	if (n >= len)
		errx(1, "link command longer than %lu bytes", (u_long)len);
}

/* load a prelinked module; returns entry point */
void *
elf_mod_load(int fd)
{
	Elf_Ehdr ehdr;
	size_t zero_size = 0;
	size_t b;
	ssize_t n;
	char *strtab;
	struct elf_section *head, *s;
	char buf[10 * BUFSIZ];
	void *addr = NULL;
		
	if (read_elf_header(fd, &ehdr) < 0)
		return NULL;

	strtab = read_string_table(fd, &ehdr);
	read_sections(fd, &ehdr, strtab, &head);
	
	for (s = head; s; s = s->next) {
		if (debug)
			fprintf(stderr, "loading `%s': addr = %p, "
				"size = %#lx\n", 
				s->name, s->addr, (u_long)s->size);
		if (s->type == SHT_NOBITS)
			/* skip some space */
			zero_size += s->size;
		else {
			if (addr != NULL) {
				/* if there is a gap in the prelinked
                                   module, transfer some empty
                                   space... */
				zero_size += (char*)s->addr - (char*)addr;
			}
			if (zero_size) {
				loadspace(zero_size);
				zero_size = 0;
			}
			b = s->size;
			if (lseek(fd, s->offset, SEEK_SET) == -1)
				err(1, "lseek");
			while (b) {
				n = read(fd, buf, MIN(b, sizeof(buf)));
				if (n == 0)
					errx(1, "unexpected EOF");
				if (n < 0)
					err(1, "read");
				loadbuf(buf, n);
				b -= n;
			}
			addr = (char*)s->addr + s->size;
		}
	}
	if (zero_size)
		loadspace(zero_size);
	
	free_sections(head);
	free(strtab);
	return (void*)ehdr.e_entry;
}

void
elf_mod_symload(strtablen)
	int strtablen;
{

	warnx("ELF can not load symbols");
}

/*	$NetBSD: map_object.c,v 1.11.4.1 2001/12/09 17:20:35 he Exp $	 */

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
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
 *      This product includes software developed by John Polstra.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "rtld.h"

static int protflags __P((int));	/* Elf flags -> mmap protection */

/*
 * Map a shared object into memory.  The argument is a file descriptor,
 * which must be open on the object and positioned at its beginning.
 *
 * The return value is a pointer to a newly-allocated Obj_Entry structure
 * for the shared object.  Returns NULL on failure.
 */
Obj_Entry *
_rtld_map_object(path, fd, sb)
	const char *path;
	int fd;
	const struct stat *sb;
{
	Obj_Entry      *obj;
	union {
		Elf_Ehdr hdr;
		char     buf[PAGESIZE];
	} u;
	int             nbytes;
	Elf_Phdr       *phdr;
	Elf_Phdr       *phlimit;
	Elf_Phdr       *segs[2];
	int             nsegs;
	Elf_Phdr       *phdyn;
	Elf_Phdr       *phphdr;
	Elf_Phdr       *phinterp;
	caddr_t         mapbase;
	size_t          mapsize;
	Elf_Off         base_offset;
	Elf_Addr        base_vaddr;
	Elf_Addr        base_vlimit;
	Elf_Addr	text_vlimit;
	caddr_t         base_addr;
	Elf_Off         data_offset;
	Elf_Addr        data_vaddr;
	Elf_Addr        data_vlimit;
	caddr_t         data_addr;
	caddr_t		gap_addr;
	size_t		gap_size;
#ifdef RTLD_LOADER
	Elf_Addr        clear_vaddr;
	caddr_t         clear_addr;
	size_t          nclear;
#endif

	if ((nbytes = read(fd, u.buf, PAGESIZE)) == -1) {
		_rtld_error("%s: read error: %s", path, xstrerror(errno));
		return NULL;
	}
	/* Make sure the file is valid */
	if (nbytes < sizeof(Elf_Ehdr) ||
	    memcmp(ELFMAG, u.hdr.e_ident, SELFMAG) != 0 ||
	    u.hdr.e_ident[EI_CLASS] != ELFCLASS) {
		_rtld_error("%s: unrecognized file format", path);
		return NULL;
	}
	/* Elf_e_ident includes class */
	if (u.hdr.e_ident[EI_VERSION] != EV_CURRENT ||
	    u.hdr.e_version != EV_CURRENT ||
	    u.hdr.e_ident[EI_DATA] != ELFDEFNNAME(MACHDEP_ENDIANNESS)) {
		_rtld_error("%s: Unsupported file version", path);
		return NULL;
	}
	if (u.hdr.e_type != ET_EXEC && u.hdr.e_type != ET_DYN) {
		_rtld_error("%s: Unsupported file type", path);
		return NULL;
	}
	switch (u.hdr.e_machine) {
		ELFDEFNNAME(MACHDEP_ID_CASES)
	default:
		_rtld_error("%s: Unsupported machine", path);
		return NULL;
	}

	/*
         * We rely on the program header being in the first page.  This is
         * not strictly required by the ABI specification, but it seems to
         * always true in practice.  And, it simplifies things considerably.
         */
	assert(u.hdr.e_phentsize == sizeof(Elf_Phdr));
	assert(u.hdr.e_phoff + u.hdr.e_phnum * sizeof(Elf_Phdr) <= PAGESIZE);
	assert(u.hdr.e_phoff + u.hdr.e_phnum * sizeof(Elf_Phdr) <= nbytes);

	/*
         * Scan the program header entries, and save key information.
         *
         * We rely on there being exactly two load segments, text and data,
         * in that order.
         */
	phdr = (Elf_Phdr *) (u.buf + u.hdr.e_phoff);
	phlimit = phdr + u.hdr.e_phnum;
	nsegs = 0;
	phdyn = phphdr = phinterp = NULL;
	while (phdr < phlimit) {
		switch (phdr->p_type) {
		case PT_INTERP:
			phinterp = phdr;
			break;

		case PT_LOAD:
			if (nsegs < 2)
				segs[nsegs] = phdr;
			++nsegs;
			break;

		case PT_PHDR:
			phphdr = phdr;
			break;

		case PT_DYNAMIC:
			phdyn = phdr;
			break;
		}

		++phdr;
	}
	if (phdyn == NULL) {
		_rtld_error("%s: not dynamically linked", path);
		return NULL;
	}
	if (nsegs != 2) {
		_rtld_error("%s: wrong number of segments (%d != 2)", path,
		    nsegs);
		return NULL;
	}
#ifdef __i386__
	assert(segs[0]->p_align <= PAGESIZE);
	assert(segs[1]->p_align <= PAGESIZE);
#endif

	/*
	 * Map the entire address space of the object as a file
	 * region to stake out our contiguous region and establish a
	 * base for relocation.  We use a file mapping so that
	 * the kernel will give us whatever alignment is appropriate
	 * for the platform we're running on.
	 *
	 * We map it using the text protection, map the data segment
	 * into the right place, then map an anon segment for the bss
	 * and unmap the gaps left by padding to alignment.
	 */

	base_offset = round_down(segs[0]->p_offset);
	base_vaddr = round_down(segs[0]->p_vaddr);
	base_vlimit = round_up(segs[1]->p_vaddr + segs[1]->p_memsz);
	text_vlimit = round_up(segs[0]->p_vaddr + segs[0]->p_memsz);
	mapsize = base_vlimit - base_vaddr;

#ifdef RTLD_LOADER
	base_addr = u.hdr.e_type == ET_EXEC ? (caddr_t) base_vaddr : NULL;
#else
	base_addr = NULL;
#endif

	mapbase = mmap(base_addr, mapsize, protflags(segs[0]->p_flags),
		       MAP_FILE | MAP_PRIVATE, fd, base_offset);
	if (mapbase == MAP_FAILED) {
		_rtld_error("mmap of entire address space failed: %s",
		    xstrerror(errno));
		return NULL;
	}

	base_addr = mapbase;

	/* Overlay the data segment onto the proper region. */
	data_offset = round_down(segs[1]->p_offset);
	data_vaddr = round_down(segs[1]->p_vaddr);
	data_vlimit = round_up(segs[1]->p_vaddr + segs[1]->p_filesz);
	data_addr = mapbase + (data_vaddr - base_vaddr);
	if (mmap(data_addr, data_vlimit - data_vaddr,
		 protflags(segs[1]->p_flags),
		 MAP_FILE | MAP_PRIVATE | MAP_FIXED, fd, data_offset)
	    == MAP_FAILED) {
		_rtld_error("mmap of data failed: %s", xstrerror(errno));
		munmap(mapbase, mapsize);
		return NULL;
	}

	/* Overlay the bss segment onto the proper region. */
	if (mmap(mapbase + data_vlimit - base_vaddr, base_vlimit - data_vlimit,
		 protflags(segs[1]->p_flags),
		 MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0)
	    == MAP_FAILED) {
		_rtld_error("mmap of bss failed: %s", xstrerror(errno));
		munmap(mapbase, mapsize);
		return NULL;
	}

	/* Unmap the gap between the text and data. */
	gap_addr = base_addr + round_up(text_vlimit - base_vaddr);
	gap_size = data_addr - gap_addr;
	if (gap_size != 0 && munmap(gap_addr, gap_size) == -1) {
		_rtld_error("munmap of text -> data gap failed: %s",
		    xstrerror(errno));
		munmap(mapbase, mapsize);
		return NULL;
	}

#ifdef RTLD_LOADER
	/* Clear any BSS in the last page of the data segment. */
	clear_vaddr = segs[1]->p_vaddr + segs[1]->p_filesz;
	clear_addr = mapbase + (clear_vaddr - base_vaddr);
	if ((nclear = data_vlimit - clear_vaddr) > 0)
		memset(clear_addr, 0, nclear);

	/* Non-file portion of BSS mapped above. */
#endif

	obj = _rtld_obj_new();
	if (sb != NULL) {
		obj->dev = sb->st_dev;
		obj->ino = sb->st_ino;
	}
	obj->mapbase = mapbase;
	obj->mapsize = mapsize;
	obj->textsize = round_up(segs[0]->p_vaddr + segs[0]->p_memsz) -
	    base_vaddr;
	obj->vaddrbase = base_vaddr;
	obj->relocbase = mapbase - base_vaddr;
	obj->dynamic = (Elf_Dyn *)(obj->relocbase + phdyn->p_vaddr);
	if (u.hdr.e_entry != 0)
		obj->entry = (caddr_t)(obj->relocbase + u.hdr.e_entry);
	if (phphdr != NULL) {
		obj->phdr = (const Elf_Phdr *)
		    (obj->relocbase + phphdr->p_vaddr);
		obj->phsize = phphdr->p_memsz;
	}
	if (phinterp != NULL)
		obj->interp = (const char *) (obj->relocbase + phinterp->p_vaddr);

	return obj;
}

void
_rtld_obj_free(obj)
	Obj_Entry *obj;
{
	Objlist_Entry *elm;

	free(obj->path);
	while (obj->needed != NULL) {
		Needed_Entry *needed = obj->needed;
		obj->needed = needed->next;
		free(needed);
	}
	while (SIMPLEQ_FIRST(&obj->dldags) != NULL) {
		elm = SIMPLEQ_FIRST(&obj->dldags);
		SIMPLEQ_REMOVE_HEAD(&obj->dldags, elm, link);
		free(elm);
	}
	while (SIMPLEQ_FIRST(&obj->dagmembers) != NULL) {
		elm = SIMPLEQ_FIRST(&obj->dagmembers);
		SIMPLEQ_REMOVE_HEAD(&obj->dagmembers, elm, link);
		free(elm);
	}
	free(obj);
}

Obj_Entry *
_rtld_obj_new(void)
{
	Obj_Entry *obj;

	obj = CNEW(Obj_Entry);
	SIMPLEQ_INIT(&obj->dldags);
	SIMPLEQ_INIT(&obj->dagmembers);
	return obj;
}

/*
 * Given a set of ELF protection flags, return the corresponding protection
 * flags for MMAP.
 */
static int
protflags(elfflags)
	int elfflags;
{
	int prot = 0;
	if (elfflags & PF_R)
		prot |= PROT_READ;
#ifdef RTLD_LOADER
	if (elfflags & PF_W)
		prot |= PROT_WRITE;
#endif
	if (elfflags & PF_X)
		prot |= PROT_EXEC;
	return prot;
}

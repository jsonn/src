/* -*-C++-*-	$NetBSD: load_elf.h,v 1.7.78.1 2008/05/16 02:22:24 yamt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#ifndef _HPCBOOT_LOAD_ELF_H_
#define	_HPCBOOT_LOAD_ELF_H_

#include <exec_elf.h>

class ElfLoader : public Loader {
private:
	Elf_Ehdr _eh;
	Elf_Phdr *_ph;
	Elf_Shdr *_sh;

	struct _symbol_block {
		BOOL enable;
		char *header;
		size_t header_size;
		Elf_Shdr *shstr;
		off_t stroff;
		Elf_Shdr *shsym;
		off_t symoff;
	} _sym_blk;

	BOOL is_elf_file(void) {
		return
		    _eh.e_ident[EI_MAG0] == ELFMAG0 &&
		    _eh.e_ident[EI_MAG1] == ELFMAG1 &&
		    _eh.e_ident[EI_MAG2] == ELFMAG2 &&
		    _eh.e_ident[EI_MAG3] == ELFMAG3;
	}
	BOOL read_header(void);
	struct PageTag *load_page(vaddr_t, off_t, size_t, struct PageTag *);
	size_t symbol_block_size(void);
	void load_symbol_block(vaddr_t);

public:
	ElfLoader(Console *&, MemoryManager *&);
	virtual ~ElfLoader(void);

	BOOL setFile(File *&);
	size_t memorySize(void);
	BOOL load(void);
	kaddr_t jumpAddr(void);
};

#endif //_HPCBOOT_LOAD_ELF_H_

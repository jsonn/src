/*	$NetBSD: headers.c,v 1.4.4.1 1999/12/27 18:30:14 wrstuden Exp $	 */

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

/*
 * Dynamic linker for ELF.
 *
 * John Polstra <jdp@polstra.com>.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <dirent.h>

#include "debug.h"
#include "rtld.h"

/*
 * Process a shared object's DYNAMIC section, and save the important
 * information in its Obj_Entry structure.
 */
void
_rtld_digest_dynamic(obj)
	Obj_Entry *obj;
{
	Elf_Dyn        *dynp;
	Needed_Entry  **needed_tail = &obj->needed;
	const Elf_Dyn  *dyn_rpath = NULL;
	Elf_Sword	plttype = DT_REL;
	Elf_Word        relsz = 0, relasz = 0;
	Elf_Word	pltrelsz = 0, pltrelasz = 0;

	for (dynp = obj->dynamic; dynp->d_tag != DT_NULL; ++dynp) {
		switch (dynp->d_tag) {

		case DT_REL:
			obj->rel = (const Elf_Rel *)
			    (obj->relocbase + dynp->d_un.d_ptr);
			break;

		case DT_RELSZ:
			relsz = dynp->d_un.d_val;
			break;

		case DT_RELENT:
			assert(dynp->d_un.d_val == sizeof(Elf_Rel));
			break;

		case DT_JMPREL:
			if (plttype == DT_REL) {
				obj->pltrel = (const Elf_Rel *)
				    (obj->relocbase + dynp->d_un.d_ptr);
			} else {
				obj->pltrela = (const Elf_RelA *)
				    (obj->relocbase + dynp->d_un.d_ptr);
			}
			break;

		case DT_PLTRELSZ:
			if (plttype == DT_REL) {
				pltrelsz = dynp->d_un.d_val;
			} else {
				pltrelasz = dynp->d_un.d_val;
			}
			break;

		case DT_RELA:
			obj->rela = (const Elf_RelA *)
			    (obj->relocbase + dynp->d_un.d_ptr);
			break;

		case DT_RELASZ:
			relasz = dynp->d_un.d_val;
			break;

		case DT_RELAENT:
			assert(dynp->d_un.d_val == sizeof(Elf_RelA));
			break;

		case DT_PLTREL:
			plttype = dynp->d_un.d_val;
			assert(plttype == DT_REL ||
			    plttype == DT_RELA);
			if (plttype == DT_RELA) {
				obj->pltrela = (const Elf_RelA *) obj->pltrel;
				obj->pltrel = NULL;
				pltrelasz = pltrelsz;
				pltrelsz = 0;
			}
			break;

		case DT_SYMTAB:
			obj->symtab = (const Elf_Sym *)
				(obj->relocbase + dynp->d_un.d_ptr);
			break;

		case DT_SYMENT:
			assert(dynp->d_un.d_val == sizeof(Elf_Sym));
			break;

		case DT_STRTAB:
			obj->strtab = (const char *)
			    (obj->relocbase + dynp->d_un.d_ptr);
			break;

		case DT_STRSZ:
			obj->strsize = dynp->d_un.d_val;
			break;

		case DT_HASH:
			{
				const Elf_Word *hashtab = (const Elf_Word *)
				(obj->relocbase + dynp->d_un.d_ptr);

				obj->nbuckets = hashtab[0];
				obj->nchains = hashtab[1];
				obj->buckets = hashtab + 2;
				obj->chains = obj->buckets + obj->nbuckets;
			}
			break;

		case DT_NEEDED:
			assert(!obj->rtld);
			{
				Needed_Entry *nep = NEW(Needed_Entry);

				nep->name = dynp->d_un.d_val;
				nep->obj = NULL;
				nep->next = NULL;

				*needed_tail = nep;
				needed_tail = &nep->next;
			}
			break;

		case DT_PLTGOT:
			obj->pltgot = (Elf_Addr *)
			    (obj->relocbase + dynp->d_un.d_ptr);
			break;

		case DT_TEXTREL:
			obj->textrel = true;
			break;

		case DT_SYMBOLIC:
			obj->symbolic = true;
			break;

		case DT_RPATH:
			/*
		         * We have to wait until later to process this, because
			 * we might not have gotten the address of the string
			 * table yet.
		         */
			dyn_rpath = dynp;
			break;

		case DT_SONAME:
			/* Not used by the dynamic linker. */
			break;

		case DT_INIT:
			obj->init = (void (*) __P((void)))
			    (obj->relocbase + dynp->d_un.d_ptr);
			break;

		case DT_FINI:
			obj->fini = (void (*) __P((void)))
			    (obj->relocbase + dynp->d_un.d_ptr);
			break;

		case DT_DEBUG:
#ifdef RTLD_LOADER
			dynp->d_un.d_ptr = (Elf_Addr)&_rtld_debug;
#endif
			break;

#if defined(__mips__)
		case DT_MIPS_LOCAL_GOTNO:
			obj->local_gotno = dynp->d_un.d_val;
			break;

		case DT_MIPS_SYMTABNO:
			obj->symtabno = dynp->d_un.d_val;
			break;

		case DT_MIPS_GOTSYM:
			obj->gotsym = dynp->d_un.d_val;
			break;

		case DT_MIPS_RLD_MAP:
#ifdef RTLD_LOADER
			*((Elf_Addr *)(dynp->d_un.d_ptr)) = (Elf_Addr)
			    &_rtld_debug;
#endif
			break;
#endif
		}
	}

	obj->rellim = (const Elf_Rel *)((caddr_t)obj->rel + relsz);
	obj->relalim = (const Elf_RelA *)((caddr_t)obj->rela + relasz);
	obj->pltrellim = (const Elf_Rel *)((caddr_t)obj->pltrel + pltrelsz);
	obj->pltrelalim = (const Elf_RelA *)((caddr_t)obj->pltrela + pltrelasz);

	if (dyn_rpath != NULL) {
		_rtld_add_paths(&obj->rpaths, obj->strtab +
		    dyn_rpath->d_un.d_val, true);
	}
}

/*
 * Process a shared object's program header.  This is used only for the
 * main program, when the kernel has already loaded the main program
 * into memory before calling the dynamic linker.  It creates and
 * returns an Obj_Entry structure.
 */
Obj_Entry *
_rtld_digest_phdr(phdr, phnum, entry)
	const Elf_Phdr *phdr;
	int phnum;
	caddr_t entry;
{
	Obj_Entry      *obj;
	const Elf_Phdr *phlimit = phdr + phnum;
	const Elf_Phdr *ph;
	int             nsegs = 0;

	obj = _rtld_obj_new();
	for (ph = phdr; ph < phlimit; ++ph) {
		switch (ph->p_type) {

		case PT_PHDR:
			assert((const Elf_Phdr *) ph->p_vaddr == phdr);
			obj->phdr = (const Elf_Phdr *) ph->p_vaddr;
			obj->phsize = ph->p_memsz;
			break;

		case PT_INTERP:
			obj->interp = (const char *) ph->p_vaddr;
			break;

		case PT_LOAD:
			assert(nsegs < 2);
			if (nsegs == 0) {	/* First load segment */
				obj->vaddrbase = round_down(ph->p_vaddr);
				obj->mapbase = (caddr_t) obj->vaddrbase;
				obj->relocbase = obj->mapbase - obj->vaddrbase;
				obj->textsize = round_up(ph->p_vaddr +
				    ph->p_memsz) - obj->vaddrbase;
			} else {		/* Last load segment */
				obj->mapsize = round_up(ph->p_vaddr +
				    ph->p_memsz) - obj->vaddrbase;
			}
			++nsegs;
			break;

		case PT_DYNAMIC:
			obj->dynamic = (Elf_Dyn *) ph->p_vaddr;
			break;
		}
	}
	assert(nsegs == 2);

	obj->entry = entry;
	return obj;
}

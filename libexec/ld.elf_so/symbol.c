/*	$NetBSD: symbol.c,v 1.11.2.2 2004/05/28 08:31:22 tron Exp $	 */

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * Copyright 2002 Charles M. Hannum <root@ihack.net>
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

static bool
_rtld_is_exported(const Elf_Sym *def)
{
	static Elf_Addr _rtld_exports[] = {
		(Elf_Addr)dlopen,
		(Elf_Addr)dlclose,
		(Elf_Addr)dlsym,
		(Elf_Addr)dlerror,
		(Elf_Addr)dladdr,
		0
	};
	int i;

	Elf_Addr value;
	value = (Elf_Addr)(_rtld_objself.relocbase + def->st_value);

	for (i = 0; _rtld_exports[i] != 0; i++) {
		if (value == _rtld_exports[i])
			return true;
	}
	return false;
}

/*
 * Hash function for symbol table lookup.  Don't even think about changing
 * this.  It is specified by the System V ABI.
 */
unsigned long
_rtld_elf_hash(const char *name)
{
	const unsigned char *p = (const unsigned char *) name;
	unsigned long   h = 0;
	unsigned long   g;
	unsigned long   c;

	for (; __predict_true((c = *p) != '\0'); p++) {
		h <<= 4;
		h += c;
		if ((g = h & 0xf0000000) != 0) {
			h ^= g;
			h ^= g >> 24;
		}
	}
	return (h);
}

const Elf_Sym *
_rtld_symlook_list(const char *name, unsigned long hash, const Objlist *objlist,
    const Obj_Entry **defobj_out, bool in_plt)
{
	const Elf_Sym *symp;
	const Elf_Sym *def;
	const Obj_Entry *defobj;
	const Objlist_Entry *elm;
	
	def = NULL;
	defobj = NULL;
	for (elm = SIMPLEQ_FIRST(objlist); elm; elm = SIMPLEQ_NEXT(elm, link)) {
		rdbg(("search object %p (%s)", elm->obj, elm->obj->path));
		if ((symp = _rtld_symlook_obj(name, hash, elm->obj, in_plt))
		    != NULL) {
			if ((def == NULL) ||
			    (ELF_ST_BIND(symp->st_info) != STB_WEAK)) {
				def = symp;
				defobj = elm->obj;
				if (ELF_ST_BIND(def->st_info) != STB_WEAK)
					break;
			}
		}
	}
	if (def != NULL)
		*defobj_out = defobj;
	return def;
}

/*
 * Search the symbol table of a single shared object for a symbol of
 * the given name.  Returns a pointer to the symbol, or NULL if no
 * definition was found.
 *
 * The symbol's hash value is passed in for efficiency reasons; that
 * eliminates many recomputations of the hash value.
 */
const Elf_Sym *
_rtld_symlook_obj(const char *name, unsigned long hash,
    const Obj_Entry *obj, bool in_plt)
{
	unsigned long symnum;

	for (symnum = obj->buckets[hash % obj->nbuckets];
	     symnum != ELF_SYM_UNDEFINED;
	     symnum = obj->chains[symnum]) {
		const Elf_Sym  *symp;
		const char     *strp;

		assert(symnum < obj->nchains);
		symp = obj->symtab + symnum;
		strp = obj->strtab + symp->st_name;
		rdbg(("check %s vs %s in %p", name, strp, obj));
		if (name[1] == strp[1] && !strcmp(name, strp)) {
			if (symp->st_shndx != SHN_UNDEF)
				return symp;
#ifndef __mips__
			/*
			 * XXX DANGER WILL ROBINSON!
			 * If we have a function pointer in the executable's
			 * data section, it points to the executable's PLT
			 * slot, and there is NO relocation emitted.  To make
			 * the function pointer comparable to function pointers
			 * in shared libraries, we must resolve data references
			 * in the libraries to point to PLT slots in the
			 * executable, if they exist.
			 */
			else if (!in_plt && symp->st_value != 0 &&
			     ELF_ST_TYPE(symp->st_info) == STT_FUNC)
				return symp;
#endif
			else
				return NULL;
		}
	}

	return NULL;
}

/*
 * Given a symbol number in a referencing object, find the corresponding
 * definition of the symbol.  Returns a pointer to the symbol, or NULL if
 * no definition was found.  Returns a pointer to the Obj_Entry of the
 * defining object via the reference parameter DEFOBJ_OUT.
 */
const Elf_Sym *
_rtld_find_symdef(unsigned long symnum, const Obj_Entry *refobj,
    const Obj_Entry **defobj_out, bool in_plt)
{
	const Elf_Sym  *ref;
	const Elf_Sym  *def;
	const Elf_Sym  *symp;
	const Obj_Entry *obj;
	const Obj_Entry *defobj;
	const Objlist_Entry *elm;
	const char     *name;
	unsigned long   hash;

	ref = refobj->symtab + symnum;
	name = refobj->strtab + ref->st_name;

	hash = _rtld_elf_hash(name);
	def = NULL;
	defobj = NULL;
	
	/* Look first in the referencing object if linked symbolically */
	if (refobj->symbolic) {
		symp = _rtld_symlook_obj(name, hash, refobj, in_plt);
		if (symp != NULL) {
			def = symp;
			defobj = refobj;
		}
	}
	
	/* Search all objects loaded at program start up. */
	if (def == NULL || ELF_ST_BIND(def->st_info) == STB_WEAK) {
		rdbg(("search _rtld_list_main"));
		symp = _rtld_symlook_list(name, hash, &_rtld_list_main, &obj, in_plt);
		if (symp != NULL &&
		    (def == NULL || ELF_ST_BIND(symp->st_info) != STB_WEAK)) {
			def = symp;
			defobj = obj;
		}
	}
	
	/* Search all RTLD_GLOBAL objects. */
	if (def == NULL || ELF_ST_BIND(def->st_info) == STB_WEAK) {
		rdbg(("search _rtld_list_global"));
		symp = _rtld_symlook_list(name, hash, &_rtld_list_global, &obj, in_plt);
		if (symp != NULL &&
		    (def == NULL || ELF_ST_BIND(symp->st_info) != STB_WEAK)) {
			def = symp;
			defobj = obj;
		}
	}
	
	/* Search all dlopened DAGs containing the referencing object. */
	for (elm = SIMPLEQ_FIRST(&refobj->dldags); elm; elm = SIMPLEQ_NEXT(elm, link)) {
		if (def != NULL && ELF_ST_BIND(def->st_info) != STB_WEAK)
			break;
		rdbg(("search DAG with root %p (%s)", elm->obj, elm->obj->path));
		symp = _rtld_symlook_list(name, hash, &elm->obj->dagmembers, &obj, in_plt);
		if (symp != NULL &&
		    (def == NULL || ELF_ST_BIND(symp->st_info) != STB_WEAK)) {
			def = symp;
			defobj = obj;
		}
	}
	
	/*
	 * Search the dynamic linker itself, and possibly resolve the
	 * symbol from there.  This is how the application links to
	 * dynamic linker services such as dlopen.  Only the values listed
	 * in the "_rtld_exports" array can be resolved from the dynamic linker.
	 */
	if (def == NULL || ELF_ST_BIND(def->st_info) == STB_WEAK) {
		symp = _rtld_symlook_obj(name, hash, &_rtld_objself, in_plt);
		if (symp != NULL && _rtld_is_exported(symp)) {
			def = symp;
			defobj = &_rtld_objself;
		}
	}
	
	/*
	 * If we found no definition and the reference is weak, treat the
	 * symbol as having the value zero.
	 */
	if (def == NULL && ELF_ST_BIND(ref->st_info) == STB_WEAK) {
		rdbg(("  returning _rtld_sym_zero@_rtld_objmain"));
		def = &_rtld_sym_zero;
		defobj = _rtld_objmain;
	}

	if (def != NULL)
		*defobj_out = defobj;
	else {
		rdbg(("lookup failed"));
		_rtld_error("%s: Undefined %ssymbol \"%s\" (symnum = %ld)",
		    refobj->path, in_plt ? "PLT " : "", name, symnum);
	}
	return def;
}

/*
 * Given a symbol name in a referencing object, find the corresponding
 * definition of the symbol.  Returns a pointer to the symbol, or NULL if
 * no definition was found.  Returns a pointer to the Obj_Entry of the
 * defining object via the reference parameter DEFOBJ_OUT.
 */
const Elf_Sym *
_rtld_symlook_default(const char *name, unsigned long hash,
    const Obj_Entry *refobj, const Obj_Entry **defobj_out, bool in_plt)
{
	const Elf_Sym *def;
	const Elf_Sym *symp;
	const Obj_Entry *obj;
	const Obj_Entry *defobj;
	const Objlist_Entry *elm;
	def = NULL;
	defobj = NULL;

	/* Look first in the referencing object if linked symbolically. */
	if (refobj->symbolic) {
		symp = _rtld_symlook_obj(name, hash, refobj, in_plt);
		if (symp != NULL) {
			def = symp;
			defobj = refobj;
		}
	}

	/* Search all objects loaded at program start up. */
	if (def == NULL || ELF_ST_BIND(def->st_info) == STB_WEAK) {
		symp = _rtld_symlook_list(name, hash, &_rtld_list_main, &obj, in_plt);
		if (symp != NULL &&
		  (def == NULL || ELF_ST_BIND(symp->st_info) != STB_WEAK)) {
			def = symp;
			defobj = obj;
		}
	}

	/* Search all dlopened DAGs containing the referencing object. */
	SIMPLEQ_FOREACH(elm, &refobj->dldags, link) {
		if (def != NULL && ELF_ST_BIND(def->st_info) != STB_WEAK)
			break;
		symp = _rtld_symlook_list(name, hash, &elm->obj->dagmembers, &obj,
		    in_plt);
		if (symp != NULL &&
		    (def == NULL || ELF_ST_BIND(symp->st_info) != STB_WEAK)) {
			def = symp;
			defobj = obj;
		}
	}

	/* Search all DAGs whose roots are RTLD_GLOBAL objects. */
	SIMPLEQ_FOREACH(elm, &_rtld_list_global, link) {
		if (def != NULL && ELF_ST_BIND(def->st_info) != STB_WEAK)
			break;
		symp = _rtld_symlook_list(name, hash, &elm->obj->dagmembers, &obj,
		    in_plt);
		if (symp != NULL &&
		    (def == NULL || ELF_ST_BIND(symp->st_info) != STB_WEAK)) {
			def = symp;
			defobj = obj;
		}
	}

#ifdef notyet
	/*
	 * Search the dynamic linker itself, and possibly resolve the
	 * symbol from there.  This is how the application links to
	 * dynamic linker services such as dlopen.  Only the values listed
	 * in the "exports" array can be resolved from the dynamic linker.
	 */
	if (def == NULL || ELF_ST_BIND(def->st_info) == STB_WEAK) {
		symp = _rtld_symlook_obj(name, hash, &_rtld_objself, in_plt);
		if (symp != NULL && is_exported(symp)) {
			def = symp;
			defobj = &_rtld_objself;
		}
	}
#endif

	if (def != NULL)
		*defobj_out = defobj;
	return def;
}

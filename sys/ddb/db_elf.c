/*	$NetBSD: db_elf.c,v 1.4.2.1 1998/08/12 02:57:28 eeh Exp $	*/

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>  
#include <sys/proc.h>

#include <machine/db_machdep.h>

#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>

#ifdef DB_ELF_SYMBOLS

#ifndef DB_ELFSIZE
#error Must define DB_ELFSIZE!
#endif

#define	ELFSIZE		DB_ELFSIZE

#include <sys/exec_elf.h>

#define	CONCAT(x,y)	__CONCAT(x,y)
#define	ELFDEFNAME(x)	CONCAT(ELF,CONCAT(ELFSIZE,CONCAT(_,x)))

static char *db_elf_find_strtab __P((db_symtab_t *));

#define	STAB_TO_SYMSTART(stab)	((Elf_Sym *)((stab)->start))
#define	STAB_TO_SYMEND(stab)	((Elf_Sym *)((stab)->end))
#define	STAB_TO_EHDR(stab)	((Elf_Ehdr *)((stab)->private))
#define	STAB_TO_SHDR(stab, e)	((Elf_Shdr *)((stab)->private + (e)->e_shoff))

/*
 * Find the symbol table and strings; tell ddb about them.
 */
void
X_db_sym_init(symtab, esymtab, name)
	void *symtab;		/* pointer to start of symbol table */
	void *esymtab;		/* pointer to end of string table,
				   for checking - rounded up to integer
				   boundary */
	char *name;
{
	Elf_Ehdr *elf;
	Elf_Shdr *shp;
	Elf_Sym *symp, *symtab_start, *symtab_end;
	char *strtab_start, *strtab_end;
	int i;

	if (ALIGNED_POINTER(symtab, long) == 0) {
		printf("DDB: bad symbol table start address %p\n", symtab);
		return;
	}

	symtab_start = symtab_end = NULL;
	strtab_start = strtab_end = NULL;

	/*
	 * The format of the symbols loaded by the boot program is:
	 *
	 *	Elf exec header
	 *	first section header
	 *	. . .
	 *	. . .
	 *	last section header
	 *	first symbol or string table section
	 *	. . .
	 *	. . .
	 *	last symbol or string table section
	 */

	/*
	 * Validate the Elf header.
	 */
	elf = (Elf_Ehdr *)symtab;
	if (bcmp(elf->e_ident, Elf_e_ident, Elf_e_siz) != 0)
		goto badheader;

	switch (elf->e_machine) {

	ELFDEFNAME(MACHDEP_ID_CASES)

	default:
		goto badheader;
	}

	/*
	 * We need to avoid the section header string table (small string
	 * table which names the sections).  We do this by assuming that
	 * the following two conditions will be true:
	 *
	 *	(1) .shstrtab will be smaller than one page.
	 *	(2) .strtab will be larger than one page.
	 *
	 * When we encounter what we think is the .shstrtab, we change
	 * its section type Elf_sht_null so that it will be ignored
	 * later.
	 */
	shp = (Elf_Shdr *)(symtab + elf->e_shoff);
	for (i = 0; i < elf->e_shnum; i++) {
		switch (shp[i].sh_type) {
		case Elf_sht_strtab:
			if (shp[i].sh_size < NBPG) {
				shp[i].sh_type = Elf_sht_null;
				continue;
			}
			if (strtab_start != NULL)
				goto multiple_strtab;
			strtab_start = (char *)(symtab + shp[i].sh_offset);
			strtab_end = (char *)(symtab + shp[i].sh_offset +
			    shp[i].sh_size);
			break;
		
		case Elf_sht_symtab:
			if (symtab_start != NULL)
				goto multiple_symtab;
			symtab_start = (Elf_Sym *)(symtab + shp[i].sh_offset);
			symtab_end = (Elf_Sym *)(symtab + shp[i].sh_offset +
			    shp[i].sh_size);
			break;

		default:
			/* Ignore all other sections. */
			break;
		}
	}

	/*
	 * Now, sanity check the symbols against the string table.
	 */
	if (symtab_start == NULL || strtab_start == NULL ||
	    ALIGNED_POINTER(symtab_start, long) == 0 ||
	    ALIGNED_POINTER(strtab_start, long) == 0)
		goto badheader;
	for (symp = symtab_start; symp < symtab_end; symp++)
		if (symp->st_name + strtab_start > strtab_end)
			goto badheader;

	/*
	 * Link the symbol table into the debugger.
	 */
	if (db_add_symbol_table((char *)symtab_start,
	    (char *)symtab_end, name, (char *)symtab) != -1)
		printf("[ preserving %lu bytes of %s symbol table ]\n",
		    (u_long)roundup((esymtab - symtab), sizeof(u_long)), name);
	return;

 badheader:
	printf("[ %s symbol table not valid ]\n", name);
	return;

 multiple_strtab:
	printf("[ %s has multiple string tables ]\n", name);
	return;

 multiple_symtab:
	printf("[ %s has multiple symbol tables ]\n", name);
	return;
}

/*
 * Internal helper function - return a pointer to the string table
 * for the current symbol table.
 */
static char *
db_elf_find_strtab(stab)
	db_symtab_t *stab;
{
	Elf_Ehdr *elf = STAB_TO_EHDR(stab);
	Elf_Shdr *shp = STAB_TO_SHDR(stab, elf);
	int i;

	for (i = 0; i < elf->e_shnum; i++) {
		if (shp[i].sh_type == Elf_sht_strtab)
			return (stab->private + shp[i].sh_offset);
	}

	return (NULL);
}

/*
 * Lookup the symbol with the given name.
 */
db_sym_t
X_db_lookup(stab, symstr)
	db_symtab_t *stab;
	char *symstr;
{
	Elf_Sym *symp, *symtab_start, *symtab_end;
	char *strtab;

	symtab_start = STAB_TO_SYMSTART(stab);
	symtab_end = STAB_TO_SYMEND(stab);

	strtab = db_elf_find_strtab(stab);
	if (strtab == NULL)
		return ((db_sym_t)0);

	for (symp = symtab_start; symp < symtab_end; symp++) {
		if (symp->st_name != 0 &&
		    db_eqname(strtab + symp->st_name, symstr, 0))
			return ((db_sym_t)symp);
	}

	return ((db_sym_t)0);
}

/*
 * Search for the symbol with the given address (matching within the
 * provided threshold).
 */
db_sym_t
X_db_search_symbol(symtab, off, strategy, diffp)
	db_symtab_t *symtab;
	db_addr_t off;
	db_strategy_t strategy;
	db_expr_t *diffp;		/* in/out */
{
	Elf_Sym *rsymp, *symp, *symtab_start, *symtab_end;
	db_expr_t diff = *diffp;

	symtab_start = STAB_TO_SYMSTART(symtab);
	symtab_end = STAB_TO_SYMEND(symtab);

	rsymp = NULL;

	for (symp = symtab_start; symp < symtab_end; symp++) {
		if (symp->st_name == 0)
			continue;
#if 0
		if (ELF_SYM_TYPE(symp->st_info) != Elf_estt_object &&
		    ELF_SYM_TYPE(symp->st_info) != Elf_estt_func)
			continue;
#endif

		if (off >= symp->st_value) {
			if ((off - symp->st_value) < diff) {
				diff = off - symp->st_value;
				rsymp = symp;
				if (diff == 0) {
					if (strategy == DB_STGY_PROC &&
					    ELF_SYM_TYPE(symp->st_info) ==
					      Elf_estt_func &&
					    ELF_SYM_BIND(symp->st_info) !=
					      Elf_estb_local)
						break;
					if (strategy == DB_STGY_ANY &&
					    ELF_SYM_BIND(symp->st_info) !=
					      Elf_estb_local)
						break;
				}
			} else if ((off - symp->st_value) == diff) {
				if (rsymp == NULL)
					rsymp = symp;
				else if (ELF_SYM_BIND(rsymp->st_info) ==
				      Elf_estb_local &&
				    ELF_SYM_BIND(symp->st_info) !=
				      Elf_estb_local) {
					/* pick the external symbol */
					rsymp = symp;
				}
			}
		}
	}

	if (rsymp == NULL)
		*diffp = off;
	else
		*diffp = diff;

	return ((db_sym_t)rsymp);
}

/*
 * Return the name and value for a symbol.
 */
void
X_db_symbol_values(symtab, sym, namep, valuep)
	db_symtab_t *symtab;
	db_sym_t sym;
	char **namep;
	db_expr_t *valuep;
{
	Elf_Sym *symp = (Elf_Sym *)sym;
	char *strtab;

	if (namep) {
		strtab = db_elf_find_strtab(symtab);
		if (strtab == NULL)
			*namep = NULL;
		else
			*namep = strtab + symp->st_name;
	}

	if (valuep)
		*valuep = symp->st_value;
}

/*
 * Return the file and line number of the current program counter
 * if we can find the appropriate debugging symbol.
 */
boolean_t
X_db_line_at_pc(symtab, cursym, filename, linenum, off)
	db_symtab_t *symtab;
	db_sym_t cursym;
	char **filename;
	int *linenum;
	db_expr_t off;
{

	/*
	 * XXX We don't support this (yet).
	 */
	return (FALSE);
}

/*
 * Returns the number of arguments to a function and their
 * names if we can find the appropriate debugging symbol.
 */
boolean_t
X_db_sym_numargs(symtab, cursym, nargp, argnamep)
	db_symtab_t *symtab;
	db_sym_t cursym;
	int *nargp;
	char **argnamep;
{

	/*
	 * XXX We don't support this (yet).
	 */
	return (FALSE);
}

/*
 * Initialization routine for Elf files.
 */
void
ddb_init(sym_start, sym_end)
	void *sym_start, *sym_end;
{

	if (sym_end > sym_start)
		X_db_sym_init(sym_start, sym_end, "netbsd");
}

#endif /* DB_ELF_SYMBOLS */

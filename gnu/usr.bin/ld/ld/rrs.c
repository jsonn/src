/*	$NetBSD: rrs.c,v 1.19.2.1 1998/06/10 22:20:29 tv Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>

#include "ld.h"

static struct _dynamic		rrs_dyn;		/* defined in link.h */
static struct so_debug		rrs_so_debug;		/* defined in link.h */
static struct section_dispatch_table	rrs_sdt;	/* defined in link.h */
static got_t			*rrs_got;
static jmpslot_t		*rrs_plt;		/* defined in md.h */
static struct relocation_info	*rrs_reloc;
static struct nzlist		*rrs_symbols;		/* RRS symbol table */
static char			*rrs_strtab;		/* RRS strings */
static struct rrs_hash		*rrs_hashtab;		/* RT hash table */
static struct shobj		*rrs_shobjs;
char				*rrs_search_paths;	/* `-L' RT search */
static int			rrs_search_paths_size;

static int	reserved_rrs_relocs;
static int	claimed_rrs_relocs;
static int	discarded_rrs_relocs;

static int	number_of_gotslots = 1;
static int	number_of_jmpslots = 1;
static int	number_of_rrs_hash_entries;
static int	number_of_rrs_symbols;
static int	rrs_strtab_size;
static int	rrs_symbol_size;

static int	current_jmpslot_offset;
static int	current_got_offset;
static int	max_got_offset;
static int	min_got_offset;
static int	got_origin;
static int	current_reloc_offset;
static int	current_hash_index;
int		number_of_shobjs;

/* Convert a GOT offset into a table entry */
#define GOTP(off)		((got_t *)((long)rrs_got + got_origin + (off)))

struct shobj {
	struct shobj		*next;
	struct file_entry	*entry;
};

/*
RRS text segment:
		+-------------------+  <-- sdt_rel (rrs_text_start)
		|                   |
		|    relocation     |
		|                   |
		+-------------------+  <-- <sdt>.sdt_hash
		|                   |
		|    hash buckets   |
		|                   |
		+-------------------+  <-- <sdt>.sdt_nzlist
		|                   |
		|     symbols       |
		|                   |
		+-------------------+  <-- <sdt>.sdt_strings
		|                   |
		|     strings       |
		|                   |
		+-------------------+  <-- <sdt>.sdt_sods
		|                   |
		|     shobjs        |
		|                   |
		+-------------------+
		|                   |
		|  shobjs strings   |  <-- <shobj>.sod_name
		|                   |
		+-------------------+


RRS data segment:

		+-------------------+  <-- __DYNAMIC (rrs_data_start)
		|                   |
		|     _dymamic      |
		|                   |
		+-------------------+  <-- __DYNAMIC.d_debug
		|                   |
		|    so_debug       |
		|                   |
		+-------------------+  <-- __DYNAMIC.d_un.d_sdt
		|                   |
		|       sdt         |
		|                   |
		+-------------------+  <-- sdt_got
		|                   |
		|      _GOT_        |  <-- _GLOBAL_OFFSET_TABLE_
		|                   |		( == sdt_got + got_origin)
		|                   |
		+-------------------+  <-- sdt_plt
		|                   |
		|       PLT         |
		|                   |
		+-------------------+
*/

/*
 * Add NAME to the list of needed run-time objects.
 * Return 1 if ENTRY was added to the list.
 */
int
rrs_add_shobj(entry)
	struct file_entry	*entry;
{
	struct shobj	**p;

	for (p = &rrs_shobjs; *p != NULL; p = &(*p)->next)
		if (strcmp((*p)->entry->filename, entry->filename) == 0)
			return 0;
	*p = (struct shobj *)xmalloc(sizeof(struct shobj));
	(*p)->next = NULL;
	(*p)->entry = entry;

	number_of_shobjs++;
	return 1;
}

void
alloc_rrs_reloc(entry, sp)
	struct file_entry	*entry;
	symbol			*sp;
{
#ifdef DEBUG
printf("alloc_rrs_reloc: %s in %s\n", sp->name, get_file_name(entry));
#endif
	reserved_rrs_relocs++;
}

void
alloc_rrs_segment_reloc(entry, r)
	struct file_entry	*entry;
	struct relocation_info	*r;
{
#ifdef DEBUG
printf("alloc_rrs_segment_reloc at %#x in %s\n",
	r->r_address, get_file_name(entry));
#endif
	reserved_rrs_relocs++;
}

void
alloc_rrs_jmpslot(entry, sp)
	struct file_entry	*entry;
	symbol			*sp;
{
	if (sp->flags & GS_HASJMPSLOT)
		return;

	sp->flags |= GS_HASJMPSLOT;
	number_of_jmpslots++;
	reserved_rrs_relocs++;
}

void
alloc_rrs_gotslot(entry, r, lsp)
	struct file_entry	*entry;
	struct relocation_info	*r;
	struct localsymbol	*lsp;
{
	symbol	*sp = lsp->symbol;

	if (!RELOC_EXTERN_P(r)) {

		if (sp != NULL) {
			warnx("%s: relocation for internal symbol "
			      "expected at %#x",
			      get_file_name(entry), RELOC_ADDRESS(r));
			return;
		}

		if (!RELOC_STATICS_THROUGH_GOT_P(r))
			/* No need for a GOT slot */
			return;

		if (lsp->flags & LS_HASGOTSLOT)
			return;

		lsp->flags |= LS_HASGOTSLOT;

	} else {

		if (sp == NULL) {
			warnx("%s: relocation must refer "
			      "to global symbol at %#x",
			      get_file_name(entry), RELOC_ADDRESS(r));
			return;
		}

		if (sp->alias)
			sp = sp->alias;

		if (sp->flags & GS_HASGOTSLOT)
			return;

		sp->flags |= GS_HASGOTSLOT;
	}

	number_of_gotslots++;
	reserved_rrs_relocs++;
}

void
alloc_rrs_cpy_reloc(entry, sp)
	struct file_entry	*entry;
	symbol			*sp;
{
	if (sp->flags & GS_CPYRELOCRESERVED)
		return;
#ifdef DEBUG
printf("alloc_rrs_copy: %s in %s\n", sp->name, get_file_name(entry));
#endif
	sp->flags |= GS_CPYRELOCRESERVED;
	reserved_rrs_relocs++;
}

static struct relocation_info *
rrs_next_reloc()
{
	struct relocation_info	*r;

	r = rrs_reloc + claimed_rrs_relocs++;
	if (claimed_rrs_relocs > reserved_rrs_relocs)
		errx(1, "internal error: RRS relocs exceed allocation %d",
			reserved_rrs_relocs);
	return r;
}

/*
 * Claim a RRS relocation as a result of a regular (ie. non-PIC)
 * relocation record in a rel file.
 *
 * Return 1 if the output file needs no further updating.
 * Return 0 if the relocation value pointed to by RELOCATION must
 * written to a.out.
 */
int
claim_rrs_reloc(entry, rp, sp, relocation)
	struct file_entry	*entry;
	struct relocation_info	*rp;
	symbol			*sp;
	long			*relocation;
{
	struct relocation_info	*r = rrs_next_reloc();

#if !defined(__arm32__) /* XXX spews too much on the arm32 right now. */
	if (rp->r_address < text_start + text_size)
		warnx("%s: RRS text relocation at %#x for \"%s\"",
			get_file_name(entry), rp->r_address, sp->name);
#endif

#ifdef DEBUG
printf("claim_rrs_reloc: %s in %s\n", sp->name, get_file_name(entry));
#endif
	r->r_address = rp->r_address;
	r->r_symbolnum = sp->rrs_symbolnum;

	if (link_mode & SYMBOLIC) {
		if (!sp->defined)
			warnx("Cannot reduce symbol \"%s\" in %s",
				sp->name, get_file_name(entry));
		RELOC_EXTERN_P(r) = 0;
		*relocation += sp->value;
		(void) md_make_reloc(rp, r, RELTYPE_RELATIVE);
		return 0;
	} else {
		RELOC_EXTERN_P(r) = 1;
		return md_make_reloc(rp, r, RELTYPE_EXTERN);
	}
}

/*
 * Claim a jmpslot. Setup RRS relocation if claimed for the first time.
 */
long
claim_rrs_jmpslot(entry, rp, sp, addend)
	struct file_entry	*entry;
	struct relocation_info	*rp;
	symbol			*sp;
	long			addend;
{
	struct relocation_info *r;

	if (!(sp->flags & GS_HASJMPSLOT))
		errx(1, "internal error: "
			"%s: claim_rrs_jmpslot: %s: no reservation",
			get_file_name(entry),
			sp->name);

	if (sp->jmpslot_offset != -1)
		return rrs_sdt.sdt_plt + sp->jmpslot_offset;

	sp->jmpslot_offset = current_jmpslot_offset;
	current_jmpslot_offset += sizeof(jmpslot_t);

#ifdef DEBUG
printf("claim_rrs_jmpslot: %s: %s(%d) -> offset %x\n",
	get_file_name(entry),
	sp->name, sp->rrs_symbolnum, sp->jmpslot_offset);
#endif

	if ((link_mode & SYMBOLIC) || rrs_section_type == RRS_PARTIAL) {
		if (!sp->defined)
			warnx("Cannot reduce symbol \"%s\" in %s",
				sp->name, get_file_name(entry));

		md_fix_jmpslot( rrs_plt + sp->jmpslot_offset/sizeof(jmpslot_t),
				rrs_sdt.sdt_plt + sp->jmpslot_offset,
				sp->value);
		if (rrs_section_type == RRS_PARTIAL || !JMPSLOT_NEEDS_RELOC) {
			/* PLT is self-contained */
			discarded_rrs_relocs++;
			return rrs_sdt.sdt_plt + sp->jmpslot_offset;
		}
	} else {
		md_make_jmpslot(rrs_plt + sp->jmpslot_offset/sizeof(jmpslot_t),
				sp->jmpslot_offset,
				claimed_rrs_relocs);
	}

	/*
	 * Install a run-time relocation for this PLT entry.
	 */
	r = rrs_next_reloc();

	RELOC_SYMBOL(r) = sp->rrs_symbolnum;

	r->r_address = (long)rrs_sdt.sdt_plt + sp->jmpslot_offset;

	if (link_mode & SYMBOLIC) {
		RELOC_EXTERN_P(r) = 0;
		md_make_jmpreloc(rp, r, RELTYPE_RELATIVE);
	} else {
		RELOC_EXTERN_P(r) = 1;
		md_make_jmpreloc(rp, r, 0);
	}

	return rrs_sdt.sdt_plt + sp->jmpslot_offset;
}

/*
 * Claim GOT entry for a global symbol. If this is the first relocation
 * claiming the entry, setup a RRS relocation for it.
 * Return offset into the GOT allocated to this symbol.
 */
long
claim_rrs_gotslot(entry, rp, lsp, addend)
	struct file_entry	*entry;
	struct relocation_info	*rp;
	struct localsymbol	*lsp;
	long			addend;
{
	struct relocation_info	*r;
	symbol	*sp = lsp->symbol;
	int	reloc_type = 0;

	if (sp == NULL) {
		return 0;
	}

	if (sp->alias)
		sp = sp->alias;

	if (!(sp->flags & GS_HASGOTSLOT))
		errx(1, "internal error: "
			"%s: claim_rrs_gotslot: %s: no reservation",
			get_file_name(entry), sp->name);

	if (sp->gotslot_offset != -1) {
#ifdef DIAGNOSTIC
		if (*GOTP(sp->gotslot_offset) != addend +
		    ((!(link_mode & SHAREABLE) || (link_mode & SYMBOLIC))
		       ? sp->value : 0))
			errx(1, "%s: %s: gotslot at %#x is multiple valued, "
				"*got = %#x, addend = %#x, sp->value = %#x",
				get_file_name(entry), sp->name,
				sp->gotslot_offset,
				*GOTP(sp->gotslot_offset), addend, sp->value);
#endif
		/* This symbol already passed here before. */
		return sp->gotslot_offset;
	}

	if (current_got_offset == 0)
		/* GOT offset 0 is reserved */
		current_got_offset += sizeof(got_t);

	if (current_got_offset > max_got_offset)
		errx(1, "%s: GOT overflow on symbol `%s' at %#x",
		      get_file_name(entry), sp->name, RELOC_ADDRESS(rp));

	sp->gotslot_offset = current_got_offset;
	current_got_offset += sizeof(got_t);

#ifdef DEBUG
printf("claim_rrs_gotslot: %s(%d,%#x) slot offset %#x, addend %#x\n",
	 sp->name, sp->rrs_symbolnum, sp->value, sp->gotslot_offset, addend);
#endif

	if (sp->defined &&
	    (!(link_mode & SHAREABLE) || (link_mode & SYMBOLIC))) {

		/*
		 * Reduce to just a base-relative translation.
		 */

#if defined(__arm32__) && 1 /* XXX MAGIC! */
		*GOTP(sp->gotslot_offset) = sp->value /*+ addend*/;
#else
		*GOTP(sp->gotslot_offset) = sp->value + addend;
#endif
		reloc_type = RELTYPE_RELATIVE;

	} else if ((link_mode & SYMBOLIC) || rrs_section_type == RRS_PARTIAL) {
		/*
		 * SYMBOLIC: all symbols must be known.
		 * RRS_PARTIAL: we don't link against shared objects,
		 * so again all symbols must be known.
		 */
		warnx("Cannot reduce symbol \"%s\" in %s",
		      sp->name, get_file_name(entry));

	} else {

		/*
		 * This gotslot will be updated with symbol value at run-time.
		 */

		*GOTP(sp->gotslot_offset) = addend;
	}

	if (rrs_section_type == RRS_PARTIAL) {
		/*
		 * Base address is known, gotslot should be fully
		 * relocated by now.
		 * NOTE: RRS_PARTIAL implies !SHAREABLE.
		 */
		if (!sp->defined)
			warnx("Cannot reduce symbol \"%s\" in %s",
			      sp->name, get_file_name(entry));
		discarded_rrs_relocs++;
		return sp->gotslot_offset;
	}

	/*
	 * Claim a relocation entry.
	 * If symbol is defined and in "main" (!SHAREABLE)
	 * we still put out a relocation as we cannot easily
	 * undo the allocation.
	 * `RELTYPE_RELATIVE' relocations have the external bit off
	 * as no symbol need be looked up at run-time.
	 */
	r = rrs_next_reloc();
	r->r_address = got_symbol->value + sp->gotslot_offset;
	RELOC_SYMBOL(r) = sp->rrs_symbolnum;
	RELOC_EXTERN_P(r) = !(reloc_type == RELTYPE_RELATIVE);
	md_make_gotreloc(rp, r, reloc_type);

	return sp->gotslot_offset;
}

/*
 * Claim a GOT entry for a static symbol. Return offset of the
 * allocated GOT entry. If RELOC_STATICS_THROUGH_GOT_P is in effect,
 * return the offset of the symbol with respect to the *location* of
 * the GOT.
 */
long
claim_rrs_internal_gotslot(entry, rp, lsp, addend)
	struct file_entry	*entry;
	struct relocation_info	*rp;
	struct localsymbol	*lsp;
	long			addend;
{
	struct relocation_info	*r;

#if defined(__arm32__) && 1 /* XXX MAGIC! */
	addend = lsp->nzlist.nz_value; /*XXX */
#else
	addend += lsp->nzlist.nz_value;
#endif

	if (!RELOC_STATICS_THROUGH_GOT_P(r))
		return addend - got_symbol->value;

	if (!(lsp->flags & LS_HASGOTSLOT))
		errx(1, "internal error: "
			"%s: claim_rrs_internal_gotslot at %#x: no reservation",
			get_file_name(entry), RELOC_ADDRESS(rp));

	if (lsp->gotslot_offset != -1) {
		/* Already claimed */
		if (*GOTP(lsp->gotslot_offset) != addend)
			errx(1, "%s: gotslot at %#x is multiple valued",
				get_file_name(entry), lsp->gotslot_offset);
		return lsp->gotslot_offset;
	}

	if (current_got_offset == 0)
		/* GOT offset 0 is reserved */
		current_got_offset += sizeof(got_t);

	if (current_got_offset > max_got_offset)
		errx(1, "%s: GOT overflow for relocation at %#x",
		      get_file_name(entry), RELOC_ADDRESS(rp));

	lsp->gotslot_offset = current_got_offset;
	current_got_offset += sizeof(got_t);

	*GOTP(lsp->gotslot_offset) = addend;

#ifdef DEBUG
printf("claim_rrs_internal_gotslot: %s: slot offset %#x, addend = %#x\n",
	get_file_name(entry), lsp->gotslot_offset, addend);
#endif

	if (rrs_section_type == RRS_PARTIAL) {
		discarded_rrs_relocs++;
		return lsp->gotslot_offset;
	}

	/*
	 * Relocation entry needed for this static GOT entry.
	 */
	r = rrs_next_reloc();
	r->r_address = got_symbol->value + lsp->gotslot_offset;
	RELOC_EXTERN_P(r) = 0;
	md_make_gotreloc(rp, r, RELTYPE_RELATIVE);
	return lsp->gotslot_offset;
}

void
claim_rrs_cpy_reloc(entry, rp, sp)
	struct file_entry	*entry;
	struct relocation_info	*rp;
	symbol			*sp;
{
	struct relocation_info	*r;

	if (sp->flags & GS_CPYRELOCCLAIMED)
		return;

	if (!(sp->flags & GS_CPYRELOCRESERVED))
		errx(1, "internal error: "
			"%s: claim_cpy_reloc: %s: no reservation",
			get_file_name(entry), sp->name);

#ifdef DEBUG
printf("claim_rrs_copy: %s: %s -> %x\n",
	get_file_name(entry), sp->name, sp->so_defined);
#endif

	r = rrs_next_reloc();
	sp->flags |= GS_CPYRELOCCLAIMED;
	r->r_address = rp->r_address;
	RELOC_SYMBOL(r) = sp->rrs_symbolnum;
	RELOC_EXTERN_P(r) = RELOC_EXTERN_P(rp);
	md_make_cpyreloc(rp, r);
}

void
claim_rrs_segment_reloc(entry, rp)
	struct file_entry	*entry;
	struct relocation_info	*rp;
{
	struct relocation_info	*r = rrs_next_reloc();

#ifdef DEBUG
printf("claim_rrs_segment_reloc: %s at %#x\n",
	get_file_name(entry), rp->r_address);
#endif

	r->r_address = rp->r_address;
	RELOC_TYPE(r) = RELOC_TYPE(rp);
	RELOC_EXTERN_P(r) = 0;
	md_make_reloc(rp, r, RELTYPE_RELATIVE);

}

/*
 * Fill the RRS hash table for the given symbol name.
 * NOTE: the hash value computation must match the one in rtld.
 */
void
rrs_insert_hash(cp, index)
	char	*cp;
	int	index;
{
	int		hashval = 0;
	struct rrs_hash	*hp;

	for (; *cp; cp++)
		hashval = (hashval << 1) + *cp;

	hashval = (hashval & 0x7fffffff) % rrs_sdt.sdt_buckets;

	/* Get to the bucket */
	hp = rrs_hashtab + hashval;
	if (hp->rh_symbolnum == -1) {
		/* Empty bucket, use it */
		hp->rh_symbolnum = index;
		hp->rh_next = 0;
		return;
	}

	while (hp->rh_next != 0)
		hp = rrs_hashtab + hp->rh_next;

	hp->rh_next = current_hash_index++;
	hp = rrs_hashtab + hp->rh_next;
	hp->rh_symbolnum = index;
	hp->rh_next = 0;
}

/*
 * There are two interesting cases to consider here.
 *
 * 1) No shared objects were loaded, but there were PIC input rel files.
 *    In this case we must output a _GLOBAL_OFFSET_TABLE_ but no other
 *    RRS data. Also, the entries in the GOT must be fully resolved.
 *
 * 2) It's a genuine dynamically linked program, so the whole RRS scoop
 *    goes into a.out.
 */
void
consider_rrs_section_lengths()
{
	int		n;
	struct shobj	*shp, **shpp;

#ifdef notyet
/* We run into trouble with this as long as shared object symbols
   are not checked for definitions */
	/*
	 * First, determine the real number of shared objects we need.
	 */
	for (shpp = &rrs_shobjs; *shpp; shpp = &(*shpp)->next) {
		while (*shpp && !((*shpp)->entry->flags & E_SYMBOLS_USED)) {
			if (--number_of_shobjs < 0)
				errx(1, "internal error: number_of_shobjs < 0");
			*shpp = (*shpp)->next;
		}
		if (*shpp == NULL)
			break;
	}
#endif

	/* First, determine what of the RRS we want */
	if (relocatable_output)
		rrs_section_type = RRS_NONE;
	else if (link_mode & SHAREABLE)
		rrs_section_type = RRS_FULL;
	else if (number_of_shobjs == 0 /*&& !(link_mode & DYNAMIC)*/) {
		/*
		 * First slots in both tables are reserved
		 * hence the "> 1" condition
		 */
		if (number_of_gotslots > 1 || number_of_jmpslots > 1)
			rrs_section_type = RRS_PARTIAL;
		else
			rrs_section_type = RRS_NONE;
	} else
		rrs_section_type = RRS_FULL;

	if (rrs_section_type == RRS_NONE) {
		got_symbol->defined = 0;
		if (reserved_rrs_relocs > 0)
			errx(1, "internal error: empty RRS has reservations");
		return;
	}

	rrs_symbol_size = LD_VERSION_NZLIST_P(soversion) ?
			sizeof(struct nzlist) : sizeof(struct nlist);

	/*
	 * If there is an entry point, __DYNAMIC must be referenced (usually
	 * from crt0), as this is the method used to determine whether the
	 * run-time linker must be called.
	 */
	if (!(link_mode & SHAREABLE) && !(dynamic_symbol->flags & GS_REFERENCED))
		errx(1, "No reference to __DYNAMIC");

	dynamic_symbol->flags |= GS_REFERENCED;

	if (number_of_gotslots > 1)
		got_symbol->flags |= GS_REFERENCED;


	/* Next, allocate relocs, got and plt */
	n = reserved_rrs_relocs * sizeof(struct relocation_info);
	rrs_reloc = (struct relocation_info *)xmalloc(n);
	bzero(rrs_reloc, n);

	n = number_of_gotslots * sizeof(got_t);
	rrs_got = (got_t *)xmalloc(n);
	bzero(rrs_got, n);

	n = number_of_jmpslots * sizeof(jmpslot_t);
	rrs_plt = (jmpslot_t *)xmalloc(n);
	bzero(rrs_plt, n);

	/* Initialize first jmpslot */
	md_fix_jmpslot(rrs_plt, 0, 0);

	if (rrs_section_type == RRS_PARTIAL) {
		rrs_data_size = number_of_gotslots * sizeof(got_t);
		rrs_data_size += number_of_jmpslots * sizeof(jmpslot_t);
		return;
	}

	/*
	 * Walk the symbol table, assign RRS symbol numbers
	 * and calculate string space.
	 * Assign number 0 to __DYNAMIC (!! Sun compatibility)
	 */
	dynamic_symbol->rrs_symbolnum = number_of_rrs_symbols++;
	FOR_EACH_SYMBOL(i ,sp) {
		if ((link_mode & SHAREABLE) && sp->warning) {
			/* Allocate N_WARNING & co */
			rrs_strtab_size +=
				2 + strlen(sp->name) + strlen(sp->warning);
			number_of_rrs_symbols += 2;
		}

		if (!(sp->flags & GS_REFERENCED))
			continue;

		rrs_strtab_size += 1 + strlen(sp->name);
		if (sp != dynamic_symbol)
			sp->rrs_symbolnum = number_of_rrs_symbols++;
		if (sp->alias) {
			/*
			 * (sigh) Always allocate space to hold the
			 * indirection. At this point there's not
			 * enough information to decide whether it's
			 * actually needed or not.
			 */
			number_of_rrs_symbols++;
			rrs_strtab_size += 1 + strlen(sp->alias->name);
		}
	} END_EACH_SYMBOL;

	/*
	 * Now that we know how many RRS symbols there are going to be,
	 * allocate and initialize the RRS symbol hash table.
	 */
	rrs_sdt.sdt_buckets = number_of_rrs_symbols/4;
	if (rrs_sdt.sdt_buckets < 4)
		rrs_sdt.sdt_buckets = 4;

	number_of_rrs_hash_entries = rrs_sdt.sdt_buckets +
				     number_of_rrs_symbols;
	rrs_hashtab = (struct rrs_hash *)xmalloc(
			number_of_rrs_hash_entries * sizeof(struct rrs_hash));
	for (n = 0; n < rrs_sdt.sdt_buckets; n++)
		rrs_hashtab[n].rh_symbolnum = -1;
	current_hash_index = rrs_sdt.sdt_buckets;

	/*
	 * Get symbols into hash table now, so we can fine tune the size
	 * of the latter. We adjust the value of `number_of_rrs_hash_entries'
	 * to the number of hash link slots actually used.
	 */
	FOR_EACH_SYMBOL(i ,sp) {
		if (sp->flags & GS_REFERENCED)
			rrs_insert_hash(sp->name, sp->rrs_symbolnum);
	} END_EACH_SYMBOL;
	number_of_rrs_hash_entries = current_hash_index;

	/*
	 * Calculate RRS section sizes.
	 */
	rrs_data_size = sizeof(struct _dynamic);
	rrs_data_size += sizeof(struct so_debug);
	rrs_data_size += sizeof(struct section_dispatch_table);
	rrs_data_size += number_of_gotslots * sizeof(got_t);
	rrs_data_size += number_of_jmpslots * sizeof(jmpslot_t);
	rrs_data_size = MALIGN(rrs_data_size);

	rrs_text_size = reserved_rrs_relocs * sizeof(struct relocation_info);
	rrs_text_size += number_of_rrs_hash_entries * sizeof(struct rrs_hash);
	rrs_text_size += number_of_rrs_symbols * rrs_symbol_size;
	rrs_search_paths_size = rrs_search_paths
					? strlen(rrs_search_paths) + 1
					: 0;
	rrs_search_paths_size = MALIGN(rrs_search_paths_size);
	rrs_text_size += rrs_search_paths_size;

	/* Align strings size */
	rrs_strtab_size = MALIGN(rrs_strtab_size);
	rrs_text_size += rrs_strtab_size;

	/* Process needed shared objects */
	for (shp = rrs_shobjs; shp; shp = shp->next) {
		char	*name = shp->entry->local_sym_name;

		if (*name == '-' && *(name+1) == 'l')
			name += 2;

		rrs_text_size += sizeof(struct sod);
		rrs_text_size += 1 + strlen(name);
	}

	/* Finally, align size */
	rrs_text_size = MALIGN(rrs_text_size);
}

void
relocate_rrs_addresses()
{
	int gotsize;

	dynamic_symbol->value = 0;

	/*
	 * Get ready to allocate linkage table offsets.
	 * First jmpslot is reserved for the run-time binder
	 * GOT entry at offset 0 is reserved for `__DYNAMIC'.
	 */
	current_jmpslot_offset = sizeof(jmpslot_t);
	current_got_offset = 0;
	max_got_offset = MAX_GOTOFF(pic_type);
	min_got_offset = MIN_GOTOFF(pic_type);
	gotsize = number_of_gotslots * sizeof(got_t);

	if (gotsize + min_got_offset - (int)sizeof(got_t) > max_got_offset)
		warnx("Global Offset Table overflow (use `-fPIC')");

	if (gotsize > max_got_offset)
		/* Position at "two-complements" origin */
		current_got_offset += min_got_offset;

	got_origin = -current_got_offset;

	if (rrs_section_type == RRS_NONE)
		return;

	if (rrs_section_type == RRS_PARTIAL) {
		rrs_sdt.sdt_got = rrs_data_start;
		got_symbol->value = rrs_sdt.sdt_got + got_origin;
		rrs_sdt.sdt_plt = rrs_sdt.sdt_got +
				  number_of_gotslots * sizeof(got_t);
		return;
	}

	/*
	 * RRS data relocations.
	 */
	rrs_dyn.d_version = soversion;
	rrs_dyn.d_debug = (struct so_debug *)
			  (rrs_data_start + sizeof(struct _dynamic));
	rrs_dyn.d_un.d_sdt = (struct section_dispatch_table *)
			     ((long)rrs_dyn.d_debug + sizeof(struct so_debug));

	rrs_sdt.sdt_got = (long)rrs_dyn.d_un.d_sdt +
			  sizeof(struct section_dispatch_table);
	rrs_sdt.sdt_plt = rrs_sdt.sdt_got + number_of_gotslots*sizeof(got_t);

	/*
	 * RRS text relocations.
	 */
	rrs_sdt.sdt_rel = rrs_text_start;
	/*
	 * Sun BUG compatibility alert.
	 * Main program's RRS text values are relative to TXTADDR? WHY??
	 */
#ifdef SUN_COMPAT
	if (soversion == LD_VERSION_SUN && !(link_mode & SHAREABLE))
		rrs_sdt.sdt_rel -= N_TXTADDR(outheader);
#endif

	rrs_sdt.sdt_hash = rrs_sdt.sdt_rel +
			reserved_rrs_relocs * sizeof(struct relocation_info);
	rrs_sdt.sdt_nzlist = rrs_sdt.sdt_hash +
			number_of_rrs_hash_entries * sizeof(struct rrs_hash);
	rrs_sdt.sdt_strings = rrs_sdt.sdt_nzlist +
			number_of_rrs_symbols * rrs_symbol_size;
	rrs_sdt.sdt_paths = rrs_search_paths
				? rrs_sdt.sdt_strings + rrs_strtab_size
				: 0;
	rrs_sdt.sdt_sods = rrs_shobjs
				? rrs_sdt.sdt_strings + rrs_strtab_size +
				  rrs_search_paths_size
				: 0;
	rrs_sdt.sdt_filler2 = 0;
	rrs_sdt.sdt_str_sz = rrs_strtab_size;
	rrs_sdt.sdt_text_sz = text_size;
	rrs_sdt.sdt_plt_sz = number_of_jmpslots * sizeof(jmpslot_t);

	/*
	 * Assign addresses to _GLOBAL_OFFSET_TABLE_ and __DYNAMIC.
	 * The value `&__DYNAMIC' is in the GOT table at offset 0.
	 */
	got_symbol->value = rrs_sdt.sdt_got + got_origin;
	*GOTP(0) = dynamic_symbol->value = rrs_data_start;

}

void
write_rrs_data()
{
	long	pos;

	if (rrs_section_type == RRS_NONE)
		return;

	pos = rrs_data_start + (N_DATOFF(outheader) - DATA_START(outheader));
	if (fseek(outstream, pos, SEEK_SET) != 0)
		err(1, "write_rrs_data: fseek");

	if (rrs_section_type == RRS_PARTIAL) {
		/*
		 * Only a GOT and PLT are needed.
		 */
		md_swapout_got(rrs_got, number_of_gotslots);
		mywrite(rrs_got, number_of_gotslots, sizeof(got_t), outstream);

		md_swapout_jmpslot(rrs_plt, number_of_jmpslots);
		mywrite(rrs_plt, number_of_jmpslots,
			sizeof(jmpslot_t), outstream);

		return;
	}

	md_swapout__dynamic(&rrs_dyn);
	mywrite(&rrs_dyn, 1, sizeof(struct _dynamic), outstream);

	md_swapout_so_debug(&rrs_so_debug);
	mywrite(&rrs_so_debug, 1, sizeof(struct so_debug), outstream);

	md_swapout_section_dispatch_table(&rrs_sdt);
	mywrite(&rrs_sdt, 1, sizeof(struct section_dispatch_table), outstream);

	md_swapout_got(rrs_got, number_of_gotslots);
	mywrite(rrs_got, number_of_gotslots, sizeof(got_t), outstream);

	md_swapout_jmpslot(rrs_plt, number_of_jmpslots);
	mywrite(rrs_plt, number_of_jmpslots, sizeof(jmpslot_t), outstream);
}

void
write_rrs_text()
{
	long			pos;
	int			i;
	int			symsize;
	struct nzlist		*nlp;
	int			offset = 0;
	struct shobj		*shp;
	struct sod		*sodp;
	int			bind;

	if (rrs_section_type == RRS_PARTIAL)
		return;

	pos = rrs_text_start + (N_TXTOFF(outheader) - TEXT_START(outheader));
	if (fseek(outstream, pos, SEEK_SET) != 0)
		err(1, "write_rrs_text: fseek");

	/* Write relocation records */
	md_swapout_reloc(rrs_reloc, reserved_rrs_relocs);
	mywrite(rrs_reloc, reserved_rrs_relocs,
		sizeof(struct relocation_info), outstream);

	/* Write the RRS symbol hash tables */
	md_swapout_rrs_hash(rrs_hashtab, number_of_rrs_hash_entries);
	mywrite(rrs_hashtab, number_of_rrs_hash_entries,
		sizeof(struct rrs_hash), outstream);

	/*
	 * Determine size of an RRS symbol entry, allocate space
	 * to collect them in.
	 */
	symsize = number_of_rrs_symbols * rrs_symbol_size;
	nlp = rrs_symbols = (struct nzlist *)alloca(symsize);
	rrs_strtab = (char *)alloca(rrs_strtab_size);

#define INCR_NLP(p)	((p) = (struct nzlist *)((long)(p) + rrs_symbol_size))

	/* __DYNAMIC symbol *must* be first for Sun compatibility */
	nlp->nz_desc = nlp->nz_other = 0;
	if (LD_VERSION_NZLIST_P(soversion))
		nlp->nz_size = 0;
	nlp->nz_type = dynamic_symbol->defined;
	nlp->nz_value = dynamic_symbol->value;
	nlp->nz_value = dynamic_symbol->value;
	nlp->nz_strx = offset;
	strcpy(rrs_strtab + offset, dynamic_symbol->name);
	offset += 1 + strlen(dynamic_symbol->name);
	INCR_NLP(nlp);

	/*
	 * Now, for each global symbol, construct a nzlist element
	 * for inclusion in the RRS symbol table.
	 */
	FOR_EACH_SYMBOL(i, sp) {

		if (sp == dynamic_symbol)
			continue;

		if ((link_mode & SHAREABLE) && sp->warning) {
			/*
			 * Write a N_WARNING duo.
			 */
			nlp->nz_type = N_WARNING;
			nlp->nz_un.n_strx = offset;
			nlp->nz_value = 0;
			nlp->nz_other = 0;
			nlp->nz_desc = 0;
			nlp->nz_size = 0;
			strcpy(rrs_strtab + offset, sp->warning);
			offset += 1 + strlen(sp->warning);
			INCR_NLP(nlp);

			nlp->nz_type = N_UNDF + N_EXT;
			nlp->nz_un.n_strx = offset;
			nlp->nz_value = 0;
			nlp->nz_other = 0;
			nlp->nz_desc = 0;
			nlp->nz_size = 0;
			strcpy(rrs_strtab + offset, sp->name);
			offset += 1 + strlen(sp->name);
			INCR_NLP(nlp);
		}

		if (!(sp->flags & GS_REFERENCED))
			continue;

		if ((long)nlp - (long)rrs_symbols >=
		    number_of_rrs_symbols * rrs_symbol_size)
			errx(1, "internal error: "
				"rrs symbols exceed allocation %d",
				number_of_rrs_symbols);

		nlp->nz_desc = 0;
		nlp->nz_other = 0;
		if (LD_VERSION_NZLIST_P(soversion))
			nlp->nz_size = 0;

		bind = (sp->flags & GS_WEAK) ? BIND_WEAK : 0;

		if (sp->defined > 1) {
			/* defined with known type */
			if (!(link_mode & SHAREABLE) &&
			    sp->alias && sp->alias->defined > 1) {
				/*
				 * If the target of an indirect symbol has
				 * been defined and we are outputting an
				 * executable, resolve the indirection; it's
				 * no longer needed.
				 */
				nlp->nz_type = sp->alias->defined;
				nlp->nz_value = sp->alias->value;
				nlp->nz_other = N_OTHER(bind, sp->alias->aux);
			} else if (sp->defined == N_SIZE) {
				/*
				 * Make sure this symbol isn't going
				 * to define anything.
				 */
				nlp->nz_type = N_UNDF;
				nlp->nz_value = 0;
			} else {
				nlp->nz_type = sp->defined;
				nlp->nz_value = sp->value;
				nlp->nz_other = N_OTHER(bind, sp->aux);
			}
			if (LD_VERSION_NZLIST_P(soversion))
				nlp->nz_size = sp->size;
		} else if (sp->common_size) {
			/*
			 * A common definition.
			 */
			nlp->nz_type = N_UNDF | N_EXT;
			nlp->nz_value = sp->common_size;
			nlp->nz_other = N_OTHER(bind, 0);
		} else if (!sp->defined) {
			/* undefined */
			nlp->nz_type = N_UNDF | N_EXT;
			nlp->nz_value = 0;
			if (sp->so_defined && sp->jmpslot_offset != -1) {
				/*
				 * A PLT entry. The auxiliary type -- which
				 * must be AUX_FUNC -- is used by the run-time
				 * linker to unambiguously resolve function
				 * address references.
				 */
				if (sp->aux != AUX_FUNC)
					errx(1, "%s: non-function jmpslot",
						sp->name);
				nlp->nz_other = N_OTHER(bind, sp->aux);
				nlp->nz_value =
					rrs_sdt.sdt_plt + sp->jmpslot_offset;
			}
		} else
			errx(1, "internal error: %s defined in mysterious way",
			     sp->name);

		/* Set symbol's name */
		nlp->nz_strx = offset;
		strcpy(rrs_strtab + offset, sp->name);
		offset += 1 + strlen(sp->name);

		if (sp->alias) {
			/*
			 * Write an extra symbol for indirections (possibly
			 * just a dummy).
			 */
			int t = (nlp->nz_type == N_INDR + N_EXT);

			INCR_NLP(nlp);
			nlp->nz_type = N_UNDF + (t ? N_EXT : 0);
			nlp->nz_un.n_strx = offset;
			nlp->nz_value = 0;
			nlp->nz_other = 0;
			nlp->nz_desc = 0;
			nlp->nz_size = 0;
			strcpy(rrs_strtab + offset, sp->alias->name);
			offset += 1 + strlen(sp->alias->name);
		}

		INCR_NLP(nlp);

	} END_EACH_SYMBOL;

	if (MALIGN(offset) != rrs_strtab_size)
		errx(1, "internal error: "
			"inconsistent RRS string table length: %d, expected %d",
			offset, rrs_strtab_size);

	/* Write the symbol table */
	if (rrs_symbol_size == sizeof(struct nlist))
		md_swapout_symbols(rrs_symbols, number_of_rrs_symbols);
	else
		md_swapout_zsymbols(rrs_symbols, number_of_rrs_symbols);
	mywrite(rrs_symbols, symsize, 1, outstream);

	/* Write the strings */
	mywrite(rrs_strtab, rrs_strtab_size, 1, outstream);

	/* Write RT search path */
	mywrite(rrs_search_paths, rrs_search_paths_size, 1, outstream);

	/*
	 * Write the names of the shared objects needed at run-time
	 */
	pos = rrs_sdt.sdt_sods + number_of_shobjs * sizeof(struct sod);
	sodp = (struct sod *)alloca( number_of_shobjs * sizeof(struct sod));

	for (i = 0, shp = rrs_shobjs; shp; i++, shp = shp->next) {
		char	*name = shp->entry->local_sym_name;

		if (i >= number_of_shobjs)
			errx(1, "internal error: # of link objects exceeds %d",
				number_of_shobjs);

		sodp[i].sod_name = pos;
		sodp[i].sod_major = shp->entry->lib_major;
		sodp[i].sod_minor = shp->entry->lib_minor;

		if (*name == '-' && *(name+1) == 'l') {
			name += 2;
			sodp[i].sod_library = 1;
		} else
			sodp[i].sod_library = 0;

		pos += 1 + strlen(name);
		sodp[i].sod_next = (i == number_of_shobjs - 1) ? 0 :
			(rrs_sdt.sdt_sods + (i+1)*sizeof(struct sod));
	}

	if (i < number_of_shobjs)
		errx(1, "internal error: "
			"# of link objects less then expected %d",
			number_of_shobjs);

	md_swapout_sod(sodp, number_of_shobjs);
	mywrite(sodp, number_of_shobjs, sizeof(struct sod), outstream);

	for (i = 0, shp = rrs_shobjs; shp; i++, shp = shp->next) {
		char	*name = shp->entry->local_sym_name;

		if (*name == '-' && *(name+1) == 'l') {
			name += 2;
		}

		mywrite(name, strlen(name) + 1, 1, outstream);
	}
}

void
write_rrs()
{

	/*
	 * First, do some consistency checks on the RRS segment.
	 */
	if (rrs_section_type == RRS_NONE) {
		if (reserved_rrs_relocs > 1)
			errx(1, "internal error: "
				"RRS relocs in static program: %d",
				reserved_rrs_relocs-1);
		return;
	}

#ifdef DEBUG
printf("rrs_relocs: reserved %d claimed %d discarded %d, gotslots %d jmpslots %d\n",
	reserved_rrs_relocs, claimed_rrs_relocs, discarded_rrs_relocs,
	number_of_gotslots-1, number_of_jmpslots-1);
#endif

	/* Final consistency check */
	if (claimed_rrs_relocs  + discarded_rrs_relocs != reserved_rrs_relocs) {
		errx(1, "internal error: "
			"reserved relocs(%d) != claimed(%d) + discarded(%d)",
			reserved_rrs_relocs,
			claimed_rrs_relocs,
			discarded_rrs_relocs);
	}

	/* Write the RRS segments. */
	write_rrs_text ();
	write_rrs_data ();
}

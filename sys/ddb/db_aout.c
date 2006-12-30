/*	$NetBSD: db_aout.c,v 1.36.2.1 2006/12/30 20:47:49 yamt Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: db_aout.c,v 1.36.2.1 2006/12/30 20:47:49 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/db_machdep.h>

#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>

#ifdef	DB_AOUT_SYMBOLS

#include <ddb/db_aout.h>

static boolean_t db_aout_sym_init(int, void *, void *, const char *);
static db_sym_t	db_aout_lookup(db_symtab_t *, const char *);
static db_sym_t	db_aout_search_symbol(db_symtab_t *, db_addr_t, db_strategy_t,
		    db_expr_t *);
static void	db_aout_symbol_values(db_symtab_t *, db_sym_t, const char **,
		    db_expr_t *);
static boolean_t db_aout_line_at_pc(db_symtab_t *, db_sym_t, char **, int *,
		    db_expr_t);
static boolean_t db_aout_sym_numargs(db_symtab_t *, db_sym_t, int *, char **);
static void	db_aout_forall(db_symtab_t *, db_forall_func_t db_forall_func,
		    void *);
static int db_add_symbol_table(char *, char *, const char *, char *);

/* Only one symbol table, please */
static db_symtab_t db_symtabs;


const db_symformat_t db_symformat_aout = {
	"a.out",
	db_aout_sym_init,
	db_aout_lookup,
	db_aout_search_symbol,
	db_aout_symbol_values,
	db_aout_line_at_pc,
	db_aout_sym_numargs,
	db_aout_forall
};

/*
 * An a.out symbol table as loaded into the kernel debugger:
 *
 * symtab	-> size of symbol entries, in bytes
 * sp		-> first symbol entry
 *		   ...
 * ep		-> last symbol entry + 1
 * strtab	== start of string table
 *		   size of string table in bytes,
 *		   including this word
 *		-> strings
 */

/*
 * Find the symbol table and strings; tell ddb about them.
 */
static boolean_t
db_aout_sym_init(
	int symsize,		/* size of symbol table */
	void *vsymtab,		/* pointer to start of symbol table */
	void *vesymtab,		/* pointer to end of string table,
				   for checking - rounded up to integer
				   boundary */
	const char *name
)
{
	struct nlist	*sym_start, *sym_end;
	struct nlist	*sp;
	char *strtab;
	int slen, bad = 0;
	char *estrtab;

	if (ALIGNED_POINTER(vsymtab, long) == 0) {
		printf("[ %s symbol table has bad start address %p ]\n",
		    name, vsymtab);
		return (FALSE);
	}

	/*
	 * Find pointers to the start and end of the symbol entries,
	 * given a pointer to the start of the symbol table.
	 */
	sym_start = (struct nlist *)vsymtab;
	sym_end   = (struct nlist *)((char *)sym_start + symsize);

	strtab = (char *)sym_end;
	if (ALIGNED_POINTER(strtab, int) == 0) {
		printf("[ %s symbol table has bad string table address %p ]\n",
		    name, strtab);
		return (FALSE);
	}
	slen = *(int *)strtab;

	estrtab = strtab + slen;

#define	round_to_size(x) \
	(((vaddr_t)(x) + sizeof(vsize_t) - 1) & ~(sizeof(vsize_t) - 1))

	if (round_to_size(estrtab) != round_to_size(vesymtab)) {
		printf("[ %s a.out symbol table not valid ]\n", name);
		return (FALSE);
	}
#undef	round_to_size

	for (sp = sym_start; sp < sym_end; sp++) {
		int strx;
		strx = sp->n_un.n_strx;
		if (strx != 0) {
			if (strx > slen) {
				printf("[ %s has bad a.out string table "
				    "index (0x%x) ]\n", name, strx);
				sp->n_un.n_name = 0;
				bad = 1;
				continue;
			}
			sp->n_un.n_name = strtab + strx;
		}
	}

	if (bad)
		return (FALSE);

	if (db_add_symbol_table((char *)sym_start, (char *)sym_end, name,
	    NULL) !=  -1) {
		printf("[ using %ld bytes of %s a.out symbol table ]\n",
			  (long)vesymtab - (long)vsymtab, name);
		return (TRUE);
	}

	return (FALSE);
}

static db_sym_t
db_aout_lookup(db_symtab_t *stab, const char *symstr)
{
	struct nlist *sp, *ep;

	stab = &db_symtabs;

	sp = (struct nlist *)stab->start;
	ep = (struct nlist *)stab->end;

	for (; sp < ep; sp++) {
		if (sp->n_un.n_name == 0)
			continue;
		if ((sp->n_type & N_STAB) == 0 &&
		    sp->n_un.n_name != 0 &&
		    db_eqname(sp->n_un.n_name, symstr, '_'))
			return ((db_sym_t)sp);
	}
	return ((db_sym_t)0);
}

static db_sym_t
db_aout_search_symbol(db_symtab_t *symtab, db_addr_t off,
    db_strategy_t strategy, db_expr_t *diffp)
{
	unsigned int	diff = *diffp;
	struct nlist	*symp = 0;
	struct nlist	*sp, *ep;

	symtab = &db_symtabs;

	sp = (struct nlist *)symtab->start;
	ep = (struct nlist *)symtab->end;

	for (; sp < ep; sp++) {
		if (sp->n_un.n_name == 0)
			continue;
		if ((sp->n_type & N_STAB) != 0 || (sp->n_type & N_TYPE) == N_FN)
			continue;
		if (off >= sp->n_value) {
			if (off - sp->n_value < diff) {
				diff = off - sp->n_value;
				symp = sp;
				if (diff == 0 &&
				    ((strategy == DB_STGY_PROC &&
				     sp->n_type == (N_TEXT|N_EXT)) ||
				     (strategy == DB_STGY_ANY &&
				     (sp->n_type & N_EXT))))
					break;
			}
			else if (off - sp->n_value == diff) {
				if (symp == 0)
					symp = sp;
				else if ((symp->n_type & N_EXT) == 0 &&
				    (sp->n_type & N_EXT) != 0)
					/* pick the external symbol */
					symp = sp;
			}
		}
	}
	if (symp == 0)
		*diffp = off;
	else
		*diffp = diff;
	return ((db_sym_t)symp);
}

/*
 * Return the name and value for a symbol.
 */
static void
db_aout_symbol_values(db_symtab_t *symtab, db_sym_t sym,
    const char **namep, db_expr_t *valuep)
{
	struct nlist *sp;

	sp = (struct nlist *)sym;
	if (namep)
		*namep = sp->n_un.n_name;
	if (valuep)
		*valuep = sp->n_value;
}


static boolean_t
db_aout_line_at_pc(db_symtab_t *symtab, db_sym_t cursym,
    char **filename, int *linenum, db_expr_t off)
{
	struct nlist	*sp, *ep;
	unsigned long		sodiff = -1UL, lndiff = -1UL, ln = 0;
	char			*fname = NULL;

	symtab = &db_symtabs;

	sp = (struct nlist *)symtab->start;
	ep = (struct nlist *)symtab->end;

/* XXX - gcc specific */
#define NEWSRC(str)	((str) != NULL && \
			(str)[0] == 'g' && strcmp((str), "gcc_compiled.") == 0)

	for (; sp < ep; sp++) {

		/*
		 * Prevent bogus linenumbers in case module not compiled
		 * with debugging options
		 */
#if 0
		if (sp->n_value <= off && (off - sp->n_value) <= sodiff &&
			NEWSRC(sp->n_un.n_name)) {
#endif
		if ((sp->n_type & N_TYPE) == N_FN || NEWSRC(sp->n_un.n_name)) {
			sodiff = lndiff = -1UL;
			ln = 0;
			fname = NULL;
		}

		if (sp->n_type == N_SO) {
			if (sp->n_value <= off && (off - sp->n_value) < sodiff) {
				sodiff = off - sp->n_value;
				fname = sp->n_un.n_name;
			}
			continue;
		}

		if (sp->n_type != N_SLINE)
			continue;

		if (sp->n_value > off)
			break;

		if (off - sp->n_value < lndiff) {
			lndiff = off - sp->n_value;
			ln = sp->n_desc;
		}
	}

	if (fname != NULL && ln != 0) {
		*filename = fname;
		*linenum = ln;
		return TRUE;
	}

	return (FALSE);
}

static boolean_t
db_aout_sym_numargs(db_symtab_t *symtab, db_sym_t cursym, int *nargp,
    char **argnamep)
{
	struct nlist		*sp, *ep;
	u_long			addr;
	int			maxnarg = *nargp, nargs = 0;
	static char question[] = "???";

	if ((struct nlist *)cursym == NULL)
		return FALSE;

	symtab = &db_symtabs;

	addr = ((struct nlist *)cursym)->n_value;
	sp = (struct nlist *)symtab->start;
	ep = (struct nlist *)symtab->end;

	for (; sp < ep; sp++) {
		if (sp->n_type == N_FUN && sp->n_value == addr) {
			while (++sp < ep && sp->n_type == N_PSYM) {
				if (nargs >= maxnarg)
					break;
				nargs++;
				*argnamep++ = sp->n_un.n_name ?
				    sp->n_un.n_name : question;
				{
				/* XXX - remove trailers */
				char *cp = *(argnamep-1);
				while (*cp != '\0' && *cp != ':') cp++;
				if (*cp == ':') *cp = '\0';
				}
			}
			*nargp = nargs;
			return TRUE;
		}
	}
	return FALSE;
}

static void
db_aout_forall(db_symtab_t *stab, db_forall_func_t db_forall_func, void *arg)
{
	static char suffix[2];
	struct nlist *sp, *ep;

	stab = &db_symtabs;

	sp = (struct nlist *)stab->start;
	ep = (struct nlist *)stab->end;

	for (; sp < ep; sp++) {
		if (sp->n_un.n_name == 0)
			continue;
		if ((sp->n_type & N_STAB) == 0 && sp->n_un.n_name != 0) {
			suffix[1] = '\0';
			switch(sp->n_type & N_TYPE) {
			case N_ABS:
				suffix[0] = '@';
				break;
			case N_TEXT:
				suffix[0] = '*';
				break;
			case N_DATA:
				suffix[0] = '+';
				break;
			case N_BSS:
				suffix[0] = '-';
				break;
			case N_FN:
				suffix[0] = '/';
				break;
			default:
				suffix[0] = '\0';
			}
			(*db_forall_func)(stab, (db_sym_t)sp, sp->n_un.n_name,
			    suffix, '_', arg);
		}
	}
	return;
}

/*
 * Add symbol table, with given name, to symbol tables.
 */
int
db_add_symbol_table(char *start, char *end, const char *name, char *ref)
{

	db_symtabs.start = start;
	db_symtabs.end = end;
	db_symtabs.name = name;
	db_symtabs.private = ref;

	return(0);
}

#endif	/* DB_AOUT_SYMBOLS */

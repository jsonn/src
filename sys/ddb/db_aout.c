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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 *
 *	$Id: db_aout.c,v 1.7.2.1 1993/11/14 22:48:26 mycroft Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <machine/db_machdep.h>		/* data types */
#include <ddb/db_sym.h>

#ifndef	DB_NO_AOUT

#define _AOUT_INCLUDE_
#include <nlist.h>
#include <stab.h>

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
 * Find pointers to the start and end of the symbol entries,
 * given a pointer to the start of the symbol table.
 */
#define	db_get_aout_symtab(symtab, sp, ep) \
	(sp = (struct nlist *)((symtab) + 1), \
	 ep = (struct nlist *)((char *)sp + *(symtab)))

#ifdef	SYMTAB_SPACE
int db_symtabsize = SYMTAB_SPACE;
char db_symtab[SYMTAB_SPACE] = { 1 };
#endif

X_db_sym_init(symtab, esymtab, name)
	int *	symtab;		/* pointer to start of symbol table */
	char *	esymtab;	/* pointer to end of string table,
				   for checking - rounded up to integer
				   boundary */
	char *	name;
{
	register struct nlist	*sym_start, *sym_end;
	register struct nlist	*sp;
	register char *	strtab;
	register int	strlen;
	char *		estrtab;

#ifdef SYMTAB_SPACE
	if (*symtab < 4) {
		printf ("DDB: no symbols\n");
		return;
	}
#endif
        
	db_get_aout_symtab(symtab, sym_start, sym_end);

	strtab = (char *)sym_end;
	strlen = *(int *)strtab;

#ifndef	SYMTAB_SPACE
	estrtab = strtab + strlen;

#define	round_to_size(x) \
	(((vm_offset_t)(x) + sizeof(vm_size_t) - 1) & ~(sizeof(vm_size_t) - 1))

	if (round_to_size(estrtab) != round_to_size(esymtab)) {
	    db_printf("[ %s symbol table not valid ]\n", name);
	    return;
        }
#undef	round_to_size
        
#endif

	for (sp = sym_start; sp < sym_end; sp++) {
	    register int strx;
	    strx = sp->n_un.n_strx;
	    if (strx != 0) {
		if (strx > strlen) {
		    db_printf("Bad string table index (%#x)\n", strx);
		    sp->n_un.n_name = 0;
		    continue;
		}
		sp->n_un.n_name = strtab + strx;
	    }
	}

	if (db_add_symbol_table(sym_start, sym_end, name, (char *)symtab) !=  -1) {
#ifndef	SYMTAB_SPACE
                db_printf("[ preserving %d bytes of %s symbol table ]\n",
                          esymtab - (char *)symtab, name);
#endif
        }
}

db_sym_t
X_db_lookup(stab, symstr)
	db_symtab_t	*stab;
	char *		symstr;
{
	register struct nlist *sp, *ep;

	sp = (struct nlist *)stab->start;
	ep = (struct nlist *)stab->end;

	for (; sp < ep; sp++) {
	    if (sp->n_un.n_name == 0)
		continue;
	    if ((sp->n_type & N_STAB) == 0 &&
		sp->n_un.n_name != 0 &&
		db_eqname(sp->n_un.n_name, symstr, '_'))
	    {
		return ((db_sym_t)sp);
	    }
	}
	return ((db_sym_t)0);
}

db_sym_t
X_db_search_symbol(symtab, off, strategy, diffp)
	db_symtab_t *	symtab;
	register
	db_addr_t	off;
	db_strategy_t	strategy;
	db_expr_t	*diffp;		/* in/out */
{
	register unsigned int	diff = *diffp;
	register struct nlist	*symp = 0;
	register struct nlist	*sp, *ep;

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
				(strategy == DB_STGY_PROC &&
					sp->n_type == (N_TEXT|N_EXT)   ||
				strategy == DB_STGY_ANY &&
					(sp->n_type & N_EXT)))
			break;
		}
		else if (off - sp->n_value == diff) {
		    if (symp == 0)
			symp = sp;
		    else if ((symp->n_type & N_EXT) == 0 &&
				(sp->n_type & N_EXT) != 0)
			symp = sp;	/* pick the external symbol */
		}
	    }
	}
	if (symp == 0) {
	    *diffp = off;
	}
	else {
	    *diffp = diff;
	}
	return ((db_sym_t)symp);
}

/*
 * Return the name and value for a symbol.
 */
void
X_db_symbol_values(sym, namep, valuep)
	db_sym_t	sym;
	char		**namep;
	db_expr_t	*valuep;
{
	register struct nlist *sp;

	sp = (struct nlist *)sym;
	if (namep)
	    *namep = sp->n_un.n_name;
	if (valuep)
	    *valuep = sp->n_value;
}


boolean_t
X_db_line_at_pc(symtab, cursym, filename, linenum, off)
	db_symtab_t *	symtab;
	db_sym_t	cursym;
	char 		**filename;
	int 		*linenum;
	db_expr_t	off;
{
	register struct nlist	*sp, *ep;
	register struct nlist	*sym = (struct nlist *)cursym;
	unsigned long		sodiff = -1UL, lndiff = -1UL, ln = 0;
	char			*fname = NULL;

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

boolean_t
X_db_sym_numargs(symtab, cursym, nargp, argnamep)
	db_symtab_t *	symtab;
	db_sym_t	cursym;
	int		*nargp;
	char		**argnamep;
{
	register struct nlist	*sp, *ep;
	u_long			addr;
	int			maxnarg = *nargp, nargs = 0;

	if (cursym == NULL)
		return FALSE;

	addr = ((struct nlist *)cursym)->n_value;
	sp = (struct nlist *)symtab->start;
	ep = (struct nlist *)symtab->end;

	for (; sp < ep; sp++) {
	    if (sp->n_type == N_FUN && sp->n_value == addr) {
		while (++sp < ep && sp->n_type == N_PSYM) {
			if (nargs >= maxnarg)
				break;
			nargs++;
			*argnamep++ = sp->n_un.n_name?sp->n_un.n_name:"???";
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

/*
 * Initialization routine for a.out files.
 */
ddb_init()
{
#ifndef SYMTAB_SPACE
	extern char	*esym;
	extern int	end;

	if (esym > (char *)&end) {
	    X_db_sym_init((int *)&end, esym, "netbsd");
	}
#else
	X_db_sym_init (db_symtab, 0, "netbsd");
#endif
}

#endif	/* DB_NO_AOUT */

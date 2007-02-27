/*	$NetBSD: db_sym.c,v 1.54.4.1 2007/02/27 16:53:44 yamt Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: db_sym.c,v 1.54.4.1 2007/02/27 16:53:44 yamt Exp $");

#include "opt_ddbparam.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ksyms.h>

#include <machine/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>
#include <ddb/db_command.h>

static void		db_symsplit(char *, char **, char **);


#ifdef DB_AOUT_SYMBOLS
#define	TBLNAME	"netbsd"

static int using_aout_symtab;
const db_symformat_t *db_symformat;
static db_forall_func_t db_sift;
extern db_symformat_t db_symformat_aout;
#endif


/*
 * Initialize the kernel debugger by initializing the master symbol
 * table.  Note that if initializing the master symbol table fails,
 * no other symbol tables can be loaded.
 */
void
ddb_init(int symsize, void *vss, void *vse)
{
#ifdef DB_AOUT_SYMBOLS
	db_symformat = &db_symformat_aout;
	if ((*db_symformat->sym_init)(symsize, vss, vse, TBLNAME) == true) {
		using_aout_symtab = true;
		return;
	}
#endif
	ksyms_init(symsize, vss, vse);	/* Will complain if necessary */
}

bool
db_eqname(const char *src, const char *dst, int c)
{

	if (!strcmp(src, dst))
		return (true);
	if (src[0] == c)
		return (!strcmp(src+1,dst));
	return (false);
}

bool
db_value_of_name(const char *name, db_expr_t *valuep)
{
	char symbol[128];
	char *mod, *sym;
	unsigned long uval;
	long val;

#ifdef DB_AOUT_SYMBOLS
	db_sym_t	ssym;

	if (using_aout_symtab) {
		/*
		 * Cannot load symtabs in a.out kernels, so the ':'
		 * style of selecting modules is irrelevant.
		 */
		ssym = (*db_symformat->sym_lookup)(NULL, name);
		if (ssym == DB_SYM_NULL)
			return (false);
		db_symbol_values(ssym, &name, valuep);
		return (true);
	}
#endif
	(void)strlcpy(symbol, name, sizeof(symbol));
	db_symsplit(symbol, &mod, &sym);
	if (ksyms_getval(mod, sym, &uval, KSYMS_EXTERN) == 0) {
		val = (long) uval;
		*valuep = (db_expr_t)val;
		return true;
	}
	if (ksyms_getval(mod, sym, &uval, KSYMS_ANY) == 0) {
		val = (long) uval;
		*valuep = (db_expr_t)val;
		return true;
	}
	return false;
}

#ifdef DB_AOUT_SYMBOLS
/* Private structure for passing args to db_sift() from db_sifting(). */
struct db_sift_args {
	char	*symstr;
	int	mode;
};

/*
 * Does the work of db_sifting(), called once for each
 * symbol via db_forall(), prints out symbols matching
 * criteria.
 */
static void
db_sift(db_symtab_t *stab, db_sym_t sym, char *name,
    char *suffix, int prefix, void *arg)
{
	char c, sc;
	char *find, *p;
	size_t len;
	struct db_sift_args *dsa;

	dsa = (struct db_sift_args*)arg;

	find = dsa->symstr;	/* String we're looking for. */
	p = name;		/* String we're searching within. */

	/* Matching algorithm cribbed from strstr(), which is not
	   in the kernel. */
	if ((c = *find++) != 0) {
		len = strlen(find);
		do {
			do {
				if ((sc = *p++) == 0)
					return;
			} while (sc != c);
		} while (strncmp(p, find, len) != 0);
	}
	if (dsa->mode=='F')	/* ala ls -F */
		db_printf("%s%s ", name, suffix);
	else
		db_printf("%s ", name);
}
#endif

/*
 * "Sift" for a partial symbol.
 * Named for the Sun OpenPROM command ("sifting").
 * If the symbol has a qualifier (e.g., ux:vm_map),
 * then only the specified symbol table will be searched;
 * otherwise, all symbol tables will be searched..
 *
 * "mode" is how-to-display, set from modifiers.
 */
void
db_sifting(char *symstr, int mode)
{
	char *mod, *sym;

#ifdef DB_AOUT_SYMBOLS
	struct db_sift_args dsa;

	if (using_aout_symtab) {
		dsa.symstr = symstr;
		dsa.mode = mode;
		(*db_symformat->sym_forall)(NULL, db_sift, &dsa);
		db_printf("\n");
		return;
	}
#endif

	db_symsplit(symstr, &mod, &sym);
	if (ksyms_sift(mod, sym, mode) == ENODEV)
		db_error("invalid symbol table name");
}

/*
 * Find the closest symbol to val, and return its name
 * and the difference between val and the symbol found.
 */
db_sym_t
db_search_symbol(db_addr_t val, db_strategy_t strategy, db_expr_t *offp)
{
	unsigned int diff;
	unsigned long naddr;
	db_sym_t ret = DB_SYM_NULL;
	const char *mod;
	const char *sym;

#ifdef DB_AOUT_SYMBOLS
	db_expr_t newdiff;
	db_sym_t ssym;

	if (using_aout_symtab) {
		newdiff = diff = ~0;
		ssym = (*db_symformat->sym_search)
		    (NULL, val, strategy, &newdiff);
		if ((unsigned int) newdiff < diff) {
			diff = newdiff;
			ret = ssym;
		}
		*offp = diff;
		return ret;
	}
#endif

	if (ksyms_getname(&mod, &sym, (vaddr_t)val, strategy) == 0) {
		(void)ksyms_getval(mod, sym, &naddr, KSYMS_ANY);
		diff = val - (db_addr_t)naddr;
		ret = (db_sym_t)naddr;
	} else
		diff = 0;
	*offp = diff;
	return ret;
}

/*
 * Return name and value of a symbol
 */
void
db_symbol_values(db_sym_t sym, const char **namep, db_expr_t *valuep)
{
	const char *mod;

	if (sym == DB_SYM_NULL) {
		*namep = 0;
		return;
	}

#ifdef DB_AOUT_SYMBOLS
	if (using_aout_symtab) {
		db_expr_t value;
		(*db_symformat->sym_value)(NULL, sym, namep, &value);
		if (valuep)
			*valuep = value;
		return;
	}
#endif

	if (ksyms_getname(&mod, namep, (vaddr_t)sym,
	    KSYMS_ANY|KSYMS_EXACT) == 0) {
		if (valuep)
			*valuep = sym;
	} else
		*namep = NULL;
}


/*
 * Print a the closest symbol to value
 *
 * After matching the symbol according to the given strategy
 * we print it in the name+offset format, provided the symbol's
 * value is close enough (eg smaller than db_maxoff).
 * We also attempt to print [filename:linenum] when applicable
 * (eg for procedure names).
 *
 * If we could not find a reasonable name+offset representation,
 * then we just print the value in hex.  Small values might get
 * bogus symbol associations, e.g. 3 might get some absolute
 * value like _INCLUDE_VERSION or something, therefore we do
 * not accept symbols whose value is zero (and use plain hex).
 * Also, avoid printing as "end+0x????" which is useless.
 * The variable db_lastsym is used instead of "end" in case we
 * add support for symbols in loadable driver modules.
 */
extern char end[];
unsigned long	db_lastsym = (unsigned long)end;
unsigned int	db_maxoff = 0x100000;

void
db_symstr(char *buf, size_t buflen, db_expr_t off, db_strategy_t strategy)
{
	const char  *name;
	const char *mod;
	unsigned long val;

#ifdef DB_AOUT_SYMBOLS
	if (using_aout_symtab) {
		db_expr_t	d;
		char 		*filename;
		db_expr_t	value;
		int 		linenum;
		db_sym_t	cursym;

		if ((unsigned long) off <= db_lastsym) {
			cursym = db_search_symbol(off, strategy, &d);
			db_symbol_values(cursym, &name, &value);
			if (name != NULL &&
			    ((unsigned int) d < db_maxoff) &&
			    value != 0) {
				strlcpy(buf, name, buflen);
				if (d) {
					strlcat(buf, "+", buflen);
					db_format_radix(buf+strlen(buf),
					    24, d, true);
				}
				if (strategy == DB_STGY_PROC) {
					if ((*db_symformat->sym_line_at_pc)
					    (NULL, cursym, &filename,
					    &linenum, off))
						snprintf(buf + strlen(buf),
						    buflen - strlen(buf),
						    " [%s:%d]",
						    filename, linenum);
				}
				return;
			}
		}
		strlcpy(buf, db_num_to_str(off), buflen);
		return;
	}
#endif
	if (ksyms_getname(&mod, &name, (vaddr_t)off,
	    strategy|KSYMS_CLOSEST) == 0) {
		(void)ksyms_getval(mod, name, &val, KSYMS_ANY);
		if (((off - val) < db_maxoff) && val) {
			snprintf(buf, buflen, "%s:%s", mod, name);
			if (off - val) {
				strlcat(buf, "+", buflen);
				db_format_radix(buf+strlen(buf),
				    24, off - val, true);
			}
#ifdef notyet
			if (strategy & KSYMS_PROC) {
				if (ksyms_fmaddr(off, &filename, &linenum) == 0)
					snprintf(buf + strlen(buf),
					    buflen - strlen(buf),
					    " [%s:%d]", filename, linenum);
			}
#endif
			return;
		}
	}
	strlcpy(buf, db_num_to_str(off), buflen);
}

void
db_printsym(db_expr_t off, db_strategy_t strategy,
    void (*pr)(const char *, ...))
{
	const char  *name;
	const char *mod;
	unsigned long uval;
	long val;
#ifdef notyet
	char *filename;
	int  linenum;
#endif

#ifdef DB_AOUT_SYMBOLS
	if (using_aout_symtab) {
		db_expr_t	d;
		char 		*filename;
		db_expr_t	value;
		int 		linenum;
		db_sym_t	cursym;
		if ((unsigned long) off <= db_lastsym) {
			cursym = db_search_symbol(off, strategy, &d);
			db_symbol_values(cursym, &name, &value);
			if (name != NULL &&
			    ((unsigned int) d < db_maxoff) &&
			    value != 0) {
				(*pr)("%s", name);
				if (d) {
					char tbuf[24];

					db_format_radix(tbuf, 24, d, true);
					(*pr)("+%s", tbuf);
				}
				if (strategy == DB_STGY_PROC) {
					if ((*db_symformat->sym_line_at_pc)
					    (NULL, cursym, &filename,
					    &linenum, off))
						(*pr)(" [%s:%d]",
						    filename, linenum);
				}
				return;
			}
		}
		(*pr)(db_num_to_str(off));
		return;
	}
#endif
	if (ksyms_getname(&mod, &name, (vaddr_t)off,
	    strategy|KSYMS_CLOSEST) == 0) {
		(void)ksyms_getval(mod, name, &uval, KSYMS_ANY);
		val = (long) uval;
		if (((off - val) < db_maxoff) && val) {
			(*pr)("%s:%s", mod, name);
			if (off - val) {
				char tbuf[24];

				db_format_radix(tbuf, 24, off - val, true);
				(*pr)("+%s", tbuf);
			}
#ifdef notyet
			if (strategy & KSYMS_PROC) {
				if (ksyms_fmaddr(off, &filename, &linenum) == 0)
					(*pr)(" [%s:%d]", filename, linenum);
			}
#endif
			return;
		}
	}
	(*pr)(db_num_to_str(off));
	return;
}

/*
 * Splits a string in the form "mod:sym" to two strings.
 */
static void
db_symsplit(char *str, char **mod, char **sym)
{
	char *cp;

	if ((cp = strchr(str, ':')) != NULL) {
		*cp++ = '\0';
		*mod = str;
		*sym = cp;
	} else {
		*mod = NULL;
		*sym = str;
	}
}

bool
db_sym_numargs(db_sym_t cursym, int *nargp, char **argnamep)
{
#ifdef DB_AOUT_SYMBOLS
	if (using_aout_symtab)
		return ((*db_symformat->sym_numargs)(NULL, cursym, nargp,
		    argnamep));
#endif
	return (false);
}


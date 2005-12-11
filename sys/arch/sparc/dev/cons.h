/*	$NetBSD: cons.h,v 1.4.28.1 2005/12/11 10:28:26 christos Exp $	*/


/*
 * PROM I/O nodes and arguments are prepared by consinit().
 * Drivers can examine these when looking for a console device match.
 */
extern int prom_stdin_node;
extern int prom_stdout_node;
extern char prom_stdin_args[];
extern char prom_stdout_args[];

#ifdef	KGDB
struct zs_chanstate;
void zs_kgdb_init(void);
void zskgdb(struct zs_chanstate *);
#endif

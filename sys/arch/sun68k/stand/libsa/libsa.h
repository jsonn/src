/*	$NetBSD: libsa.h,v 1.3.14.1 2005/04/29 11:28:27 kent Exp $	*/

/*
 * This file defines the API for libsa.a
 * as used by the various boot programs.
 */

/*
 * Standard Sun3 PROM load address.
 * i.e. this is where the PROM loads
 * programs, like it or not!
 */
#define KERN_LOADADDR	0x4000

/* SRT0.S */
void	ICIA(void);
void **	getvbr(void);

/* SRT1.c */
extern int _is3x;
extern int _is2;
void _start(void);
void breakpoint(void);
void chain_to(void *);

/* clock.c */
extern int hz;
long getsecs(void);
long getticks(void);

/* exec_sun.c */
int exec_sun(char *, char *);
int load_sun(int, char *, char **);

/* promboot.c */
extern int debug;
extern char prom_bootdev[];
extern char *prom_bootfile;
extern int prom_boothow;

/* sun2.c */
void sun2_getidprom(u_char *);
u_long sun2_map_mem_load(void);
void *sun2_map_mem_run(void *);

/* sun3.c */
void sun3_getidprom(u_char *);

/* vers.c */
extern const char bootprog_rev[];
extern const char bootprog_name[];

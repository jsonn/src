/* $NetBSD: machdep.h,v 1.2.2.4 2002/03/16 15:56:09 jdolecek Exp $ */

#ifndef _ARM32_BOOT_MACHDEP_H_
#define _ARM32_BOOT_MACHDEP_H_

/* misc prototypes used by the many arm machdeps */
void halt __P((void));
void parse_mi_bootargs __P((char *));
void data_abort_handler __P((trapframe_t *));
void prefetch_abort_handler __P((trapframe_t *));
void undefinedinstruction_bounce __P((trapframe_t *));
void dumpsys	__P((void));

/* 
 * note that we use void * as all the platforms have different ideas on what
 * the structure is
 */
u_int initarm __P((void *));

/* from arm/arm32/intr.c */
void dosoftints __P((void));
void set_spl_masks __P((void));
#ifdef DIAGNOSTIC
void dump_spl_masks __P((void));
#endif
#endif

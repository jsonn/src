/*	$NetBSD: libsa.h,v 1.1.2.2 2002/06/23 17:36:32 jdolecek Exp $	*/

/*	$OpenBSD: libsa.h,v 1.7 1999/12/23 04:07:47 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
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

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#include <dev/cons.h>

#define	EXEC_ELF
/* #define	EXEC_ECOFF */
#define	EXEC_SOM

#define	NENTS(a)	(sizeof(a) / sizeof(a[0]))

extern dev_t bootdev;

extern	struct consdev constab[];
extern	struct consdev *cn_tab;
int	cngetc __P((void));
void	cnputc __P((int));
void	cninit __P((void));

void pdc_init __P((void));
struct pz_device;
struct pz_device *pdc_findev __P((int, int));

int iodcstrategy __P((void *, int, daddr_t, size_t, void *, size_t *));

int ctopen __P((struct open_file *, ...));
int ctclose __P((struct open_file *));

int dkopen __P((struct open_file *, ...));
int dkclose __P((struct open_file *));

int lfopen __P((struct open_file *, ...));
int lfstrategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int lfclose __P((struct open_file *));

void ite_probe __P((struct consdev *));
void ite_init __P((struct consdev *));
int ite_getc __P((dev_t));
void ite_putc __P((dev_t, int));
void ite_pollc __P((dev_t, int));

void machdep __P((void));
void devboot __P((dev_t, char *));
void fcacheall __P((void));

int     lif_open __P((char *path, struct open_file *f));
int     lif_close __P((struct open_file *f));
int     lif_read __P((struct open_file *f, void *buf,
                size_t size, size_t *resid));
int     lif_write __P((struct open_file *f, void *buf,
		size_t size, size_t *resid));
off_t   lif_seek __P((struct open_file *f, off_t offset, int where));
int     lif_stat __P((struct open_file *f, struct stat *sb));
int     lif_readdir __P((struct open_file *f, char *name));

union x_header;
struct x_param;
int	som_probe __P((int, union x_header *));
int	som_load __P((int, struct x_param *));
int	som_ldsym __P((int, struct x_param *));

extern int debug;
extern	char bootprog_name[], bootprog_rev[], bootprog_date[],
		bootprog_maker[];


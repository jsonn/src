/*	$NetBSD: extern.h,v 1.7.2.2 2000/10/19 16:28:28 he Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)extern.h	8.1 (Berkeley) 6/6/93
 */

#include <sys/cdefs.h>
#include <fcntl.h>
#include <kvm.h>

extern struct	cmdtab *curcmd;
extern struct	cmdtab cmdtab[];
extern struct	text *xtext;
extern WINDOW	*wnd;
extern char	**dr_name;
extern char	c, *namp, hostname[];
extern double	avenrun[3];
extern float	*dk_mspw;
extern kvm_t	*kd;
extern long	ntext, textp;
extern int	*dk_select;
extern int	CMDLINE;
extern int	dk_ndrive;
extern int	hz, stathz;
extern int	naptime, col;
extern int	nhosts;
extern int	nports;
extern int	protos;
extern int	verbose;

struct inpcb;

int	 checkhost __P((struct inpcb *));
int	 checkport __P((struct inpcb *));
void	 closeiostat __P((WINDOW *));
void	 closekre __P((WINDOW *));
void	 closembufs __P((WINDOW *));
void	 closenetstat __P((WINDOW *));
void	 closepigs __P((WINDOW *));
void	 closeswap __P((WINDOW *));
int	 cmdiostat __P((char *, char *));
int	 cmdkre __P((char *, char *));
int	 cmdnetstat __P((char *, char *));
struct	 cmdtab *lookup __P((char *));
void	 command __P((char *));
void	 die __P((int));
void	 display __P((int));
int	 dkinit __P((int, gid_t));
int	 dkcmd __P((char *, char *));
void	 error __P((const char *fmt, ...))
	__attribute__((__format__(__printf__, 1, 2)));
void	 fetchiostat __P((void));
void	 fetchkre __P((void));
void	 fetchmbufs __P((void));
void	 fetchnetstat __P((void));
void	 fetchpigs __P((void));
void	 fetchswap __P((void));
int	 initiostat __P((void));
int	 initkre __P((void));
int	 initmbufs __P((void));
int	 initnetstat __P((void));
int	 initpigs __P((void));
int	 initswap __P((void));
int	 keyboard __P((void)) __attribute__((__noreturn__));
ssize_t	 kvm_ckread __P((void *, void *, size_t));
void	 labeliostat __P((void));
void	 labelkre __P((void));
void	 labelmbufs __P((void));
void	 labelnetstat __P((void));
void	 labelpigs __P((void));
void	 labelps __P((void));
void	 labels __P((void));
void	 labelswap __P((void));
void	 load __P((void));
int	 netcmd __P((char *, char *));
void	 nlisterr __P((struct nlist []));
WINDOW	*openiostat __P((void));
WINDOW	*openkre __P((void));
WINDOW	*openmbufs __P((void));
WINDOW	*opennetstat __P((void));
WINDOW	*openpigs __P((void));
WINDOW	*openswap __P((void));
int	 prefix __P((char *, char *));
void	 redraw __P((int));
void	 showiostat __P((void));
void	 showkre __P((void));
void	 showmbufs __P((void));
void	 shownetstat __P((void));
void	 showpigs __P((void));
void	 showswap __P((void));
void	 showps __P((void));
void	 status __P((void));
void	 suspend __P((int));

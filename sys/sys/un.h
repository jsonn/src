/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	from: @(#)un.h	7.7 (Berkeley) 6/28/90
 *	$Id: un.h,v 1.4.4.1 1993/11/10 19:57:02 mycroft Exp $
 */

#ifndef _SYS_UN_H_
#define _SYS_UN_H_

/*
 * Definitions for system-internal IPC domain.
 */
struct	sockaddr_un {
	u_char	sun_len;		/* sockaddr len including null */
	u_char	sun_family;		/* AF_LOCAL */
	char	sun_path[104];		/* path name (gag) */
};

#ifdef KERNEL

#include <sys/unpcb.h>

int	uipc_usrreq __P((struct socket *so, int req, struct mbuf *m,
		struct mbuf *nam, struct mbuf *control));
int	unp_attach __P((struct socket *so));
int	unp_detach __P((struct unpcb *unp));
int	unp_bind __P((struct unpcb *unp, struct mbuf *nam, struct proc *p));
int	unp_connect __P((struct socket *so, struct mbuf *nam, struct proc *p));
int	unp_connect2 __P((struct socket *so, struct socket *so2));
void	unp_disconnect __P((struct unpcb *unp));
void	unp_mark __P((struct file *fp));
void	unp_discard __P((struct file *fp));
#else

/* actual length of an initialized sockaddr_un */
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

#endif /* !_SYS_UN_H_ */

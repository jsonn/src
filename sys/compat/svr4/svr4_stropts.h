/*	$NetBSD: svr4_stropts.h,v 1.6.4.1 1996/06/11 01:12:51 jtc Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
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

#ifndef	_SVR4_STROPTS_H_
#define	_SVR4_STROPTS_H_


struct svr4_strbuf {
	int	 maxlen;
	int 	 len;
	char 	*buf;
};

#define SVR4_STR             ('S' << 8)
#define SVR4_I_NREAD         (SVR4_STR| 1)
#define SVR4_I_PUSH          (SVR4_STR| 2)
#define SVR4_I_POP           (SVR4_STR| 3)
#define SVR4_I_LOOK          (SVR4_STR| 4)
#define SVR4_I_FLUSH         (SVR4_STR| 5)
#define SVR4_I_SRDOPT        (SVR4_STR| 6)
#define SVR4_I_GRDOPT        (SVR4_STR| 7)
#define SVR4_I_STR           (SVR4_STR| 8)
#define SVR4_I_SETSIG        (SVR4_STR| 9)
#define SVR4_I_GETSIG        (SVR4_STR|10)
#define SVR4_I_FIND          (SVR4_STR|11)
#define SVR4_I_LINK          (SVR4_STR|12)
#define SVR4_I_UNLINK        (SVR4_STR|13)
#define SVR4_I_ERECVFD       (SVR4_STR|14)
#define SVR4_I_PEEK          (SVR4_STR|15)
#define SVR4_I_FDINSERT      (SVR4_STR|16)
#define SVR4_I_SENDFD        (SVR4_STR|17)
#define SVR4_I_RECVFD        (SVR4_STR|18)
#define SVR4_I_SWROPT        (SVR4_STR|19)
#define SVR4_I_GWROPT        (SVR4_STR|20)
#define SVR4_I_LIST          (SVR4_STR|21)
#define SVR4_I_PLINK         (SVR4_STR|22)
#define SVR4_I_PUNLINK       (SVR4_STR|23)
#define SVR4_I_SETEV         (SVR4_STR|24)
#define SVR4_I_GETEV         (SVR4_STR|25)
#define SVR4_I_STREV         (SVR4_STR|26)
#define SVR4_I_UNSTREV       (SVR4_STR|27)
#define SVR4_I_FLUSHBAND     (SVR4_STR|28)
#define SVR4_I_CKBAND        (SVR4_STR|29)
#define SVR4_I_GETBAND       (SVR4_STR|30)
#define SVR4_I_ATMARK        (SVR4_STR|31)
#define SVR4_I_SETCLTIME     (SVR4_STR|32)
#define SVR4_I_GETCLTIME     (SVR4_STR|33)
#define SVR4_I_CANPUT        (SVR4_STR|34)

/* Struct passed for SVR4_I_STR */
struct svr4_strioctl {
	u_long	 cmd;
	int	 timeout;
	int	 len;
	char	*buf;
};


/*
 * Our internal state for the stream
 * For now we keep almost nothing... In the future we can keep more
 * streams state.
 */
struct svr4_strm {
	int	s_family;	/* socket family */
	int	s_cmd;		/* last getmsg reply or putmsg request */
};

/*
 * The following structures are determined empirically.
 */
struct svr4_strmcmd {
	long	cmd;		/* command ? 		*/
	long	len;		/* Address len 		*/
	long	offs;		/* Address offset	*/
	long	pad[61];
};

struct svr4_infocmd {
	long	cmd;
	long	tsdu;
	long	etsdu;
	long	cdata;
	long	ddata;
	long	addr;
	long	opt;
	long	tidu;
	long	serv;
	long	current;
	long	provider;
};

struct svr4_netaddr_in {
	u_short	family;
	u_short	port;
	u_long	addr;
};

struct svr4_netaddr_un {
	u_short	family;
	char 	path[1];
};

#define SVR4_ADDROF(sc) (void *) (((char *) (sc)) + (sc)->offs)

struct svr4_strm *svr4_stream_get __P((struct file *fp));

#endif /* !_SVR4_STROPTS */

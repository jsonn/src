/*	$NetBSD: smb_subr.h,v 1.11.2.2 2004/08/03 10:56:05 skrll Exp $	*/

/*
 * Copyright (c) 2000-2001, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/netsmb/smb_subr.h,v 1.4 2001/12/10 08:09:48 obrien Exp
 */
#ifndef _NETSMB_SMB_SUBR_H_
#define _NETSMB_SMB_SUBR_H_

#ifndef _KERNEL
#error "This file shouldn't be included from userland programs"
#endif

MALLOC_DECLARE(M_SMBTEMP);

#define SMBERROR(format, args...) printf("%s: "format, __func__ ,## args)
#define SMBPANIC(format, args...) printf("%s: "format, __func__ ,## args)

#ifdef SMB_SOCKET_DEBUG
#define SMBSDEBUG(format, args...) printf("%s: "format, __func__ ,## args)
#else
#define SMBSDEBUG(format, args...)
#endif

#ifdef SMB_IOD_DEBUG
#define SMBIODEBUG(format, args...) printf("%s: "format, __func__ ,## args)
#else
#define SMBIODEBUG(format, args...)
#endif

#ifdef SMB_SOCKETDATA_DEBUG
void m_dumpm(struct mbuf *m);
#else
#define m_dumpm(m)
#endif

#ifdef __NetBSD__
#define SIGISMEMBER(s,n) sigismember(&(s),n)
#endif

#define	SMB_SIGMASK(set) 						\
	(SIGISMEMBER(set, SIGINT) || SIGISMEMBER(set, SIGTERM) ||	\
	 SIGISMEMBER(set, SIGHUP) || SIGISMEMBER(set, SIGKILL) ||	\
	 SIGISMEMBER(set, SIGQUIT))

#define	smb_suser(cred)	suser(cred, 0)

/*
 * Compatibility wrappers for simple locks
 */

#define	smb_slock			simplelock
#define	smb_sl_init(mtx, desc)		simple_lock_init(mtx)
#define	smb_sl_destroy(mtx)		/*simple_lock_destroy(mtx)*/
#define	smb_sl_lock(mtx)		simple_lock(mtx)
#define	smb_sl_unlock(mtx)		simple_unlock(mtx)

#define SMB_STRFREE(p)	do { if (p) smb_strfree(p); } while(0)

typedef u_int16_t	smb_unichar;
typedef	smb_unichar	*smb_uniptr;

/*
 * Crediantials of user/process being processing in the connection procedures
 */
struct smb_cred {
	/* struct thread *	scr_td; */
	struct lwp *	scr_l;
	struct ucred *	scr_cred;
};

extern const smb_unichar smb_unieol;

struct mbchain;
struct smb_vc;
struct smb_rq;

void smb_makescred(struct smb_cred *scred, struct lwp *l, struct ucred *cred);
int  smb_proc_intr(struct lwp *);
char *smb_strdup(const char *s);
char *smb_strdupin(char *s, int maxlen);
void *smb_memdupin(void *umem, int len);
void smb_strtouni(u_int16_t *dst, const char *src);
void smb_strfree(char *s);
void smb_memfree(void *s);
void *smb_zmalloc(unsigned long size, struct malloc_type *type, int flags);

int  smb_encrypt(const u_char *apwd, u_char *C8, u_char *RN);
int  smb_ntencrypt(const u_char *apwd, u_char *C8, u_char *RN);
int  smb_maperror(int eclass, int eno);
int  smb_put_dmem(struct mbchain *mbp, struct smb_vc *vcp,
	const char *src, int len, int caseopt);
int  smb_put_dstring(struct mbchain *mbp, struct smb_vc *vcp,
	const char *src, int caseopt);
int  smb_put_string(struct smb_rq *rqp, const char *src);
#if 0
int  smb_put_asunistring(struct smb_rq *rqp, const char *src);
#endif

struct sockaddr *dup_sockaddr(struct sockaddr *, int);

#endif /* !_NETSMB_SMB_SUBR_H_ */

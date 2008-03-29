/*	$NetBSD: ibcs2_socksys.c,v 1.18.8.1 2008/03/29 20:46:58 christos Exp $	*/

/*
 * Copyright (c) 1994, 1995 Scott Bartram
 * Copyright (c) 1994 Arne H Juul
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ibcs2_socksys.c,v 1.18.8.1 2008/03/29 20:46:58 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <net/if.h>


#include <sys/syscallargs.h>

#include <compat/ibcs2/ibcs2_socksys.h>
#include <compat/ibcs2/ibcs2_util.h>

/*
 * iBCS2 socksys calls.
 */

int
ibcs2_socksys(struct lwp *l, const struct ibcs2_socksys_args *uap, register_t *retval)
{
	int error;
	int realargs[7]; /* 1 for command, 6 for recvfrom */

	/*
	 * SOCKET should only be legal on /dev/socksys.
	 * GETIPDOMAINNAME should only be legal on /dev/socksys ?
	 * The others are (and should be) only legal on sockets.
	 */

	error = copyin(uap->argsp, (void *)realargs, sizeof(realargs));
	if (error)
		return error;
	DPRINTF(("ibcs2_socksys: %08x %08x %08x %08x %08x %08x %08x\n",
	       realargs[0], realargs[1], realargs[2], realargs[3],
	       realargs[4], realargs[5], realargs[6]));
	switch (realargs[0]) {
	case SOCKSYS_ACCEPT:
		return sys_accept(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_BIND:
		return sys_bind(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_CONNECT:
		return sys_connect(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_GETPEERNAME:
		return sys_getpeername(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_GETSOCKNAME:
		return sys_getsockname(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_GETSOCKOPT:
		return sys_getsockopt(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_LISTEN:
		return sys_listen(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_RECV:
		realargs[5] = realargs[6] = 0;
		/* FALLTHROUGH */
	case SOCKSYS_RECVFROM:
		return sys_recvfrom(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SEND:
		realargs[5] = realargs[6] = 0;
		/* FALLTHROUGH */
	case SOCKSYS_SENDTO:
		return sys_sendto(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SETSOCKOPT:
		return sys_setsockopt(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SHUTDOWN:
		return sys_shutdown(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SOCKET:
		return compat_30_sys_socket(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SELECT:
		return compat_50_sys_select(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_GETIPDOMAIN:
		return compat_09_sys_getdomainname(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SETIPDOMAIN:
		return compat_09_sys_setdomainname(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_ADJTIME:
		return compat_50_sys_adjtime(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SETREUID:
		return sys_setreuid(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SETREGID:
		return sys_setregid(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_GETTIME:
		return compat_50_sys_gettimeofday(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SETTIME:
		return compat_50_sys_settimeofday(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_GETITIMER:
		return compat_50_sys_getitimer(l, (const void *)(realargs + 1), retval);
	case SOCKSYS_SETITIMER:
		return compat_50_sys_setitimer(l, (const void *)(realargs + 1), retval);

	default:
		printf("socksys unknown %08x %08x %08x %08x %08x %08x %08x\n",
		    realargs[0], realargs[1], realargs[2], realargs[3],
                    realargs[4], realargs[5], realargs[6]);
		return EINVAL;
	}
	/* NOTREACHED */
}

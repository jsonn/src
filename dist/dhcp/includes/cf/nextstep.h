/* nextstep.h

   System dependencies for NEXTSTEP 3 & 4 (tested on 4.2PR2)... */

/*
 * Copyright (c) 1996-1999 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

/* NeXT needs BSD44 ssize_t */
typedef int		ssize_t;
/* NeXT doesn't have BSD setsid() */
#define setsid getpid
#import <sys/types.h>
/* Porting::
   The jmp_buf type as declared in <setjmp.h> is sometimes a structure
   and sometimes an array.   By default, we assume it's a structure.
   If it's an array on your system, you may get compile warnings or errors
   as a result in confpars.c.   If so, try including the following definitions,
   which treat jmp_buf as an array: */
#if 0
#define jbp_decl(x)	jmp_buf x
#define jref(x)		(x)
#define jdref(x)	(x)
#define jrefproto	jmp_buf
#endif
#import <syslog.h>
#import <string.h>
#import <errno.h>
#import <unistd.h>
#import <sys/wait.h>
#import <signal.h>
#import <setjmp.h>
#import <limits.h>
extern int h_errno;
#import <net/if.h>
#import <net/if_arp.h>
/* Porting::
   Some older systems do not have defines for IP type-of-service,
   or don't define them the way we expect.   If you get undefined
   symbol errors on the following symbols, they probably need to be
   defined here. */
#if 0
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04
#endif

#if !defined (_PATH_DHCPD_PID)
# define _PATH_DHCPD_PID	"/etc/dhcpd.pid"
#endif

#if !defined (_PATH_DHCLIENT_PID)
# define _PATH_DHCLIENT_PID	"/etc/dhclient.pid"
#endif

#if !defined (_PATH_DHCRELAY_PID)
# define _PATH_DHCRELAY_PID	"/etc/dhcrelay.pid"
#endif

/* Stdarg definitions for ANSI-compliant C compilers. */
#import <stdarg.h>
#define VA_DOTDOTDOT ...
#define VA_start(list, last) va_start (list, last)
#define va_dcl
/* NeXT lacks snprintf */
#define vsnprintf(buf, size, fmt, list) vsprintf (buf, fmt, list)
#define NO_SNPRINTF
/* Porting::
   You must define the default network API for your port.   This
   will depend on whether one of the existing APIs will work for
   you, or whether you need to implement support for a new API.
   Currently, the following APIs are supported:
   	The BSD socket API: define USE_SOCKETS.
	The Berkeley Packet Filter: define USE_BPF.
	The Streams Network Interface Tap (NIT): define USE_NIT.
	Raw sockets: define USE_RAW_SOCKETS
   If your system supports the BSD socket API and doesn't provide
   one of the supported interfaces to the physical packet layer,
   you can either provide support for the low-level API that your
   system does support (if any) or just use the BSD socket interface.
   The BSD socket interface doesn't support multiple network interfaces,
   and on many systems, it does not support the all-ones broadcast
   address, which can cause problems with some DHCP clients (e.g.
   Microsoft Windows 95). */
#define USE_BPF
#if 0
#if defined (USE_DEFAULT_NETWORK)
#  define USE_SOCKETS
#endif
#endif
#define EOL '\n'
#define VOIDPTR void *
#import <time.h>
#define TIME time_t
#define GET_TIME(x)	time ((x))

#ifdef NEED_PRAND_CONF
const char *cmds[] = {
	"/bin/ps -axlw 2>&1",
	"/usr/etc/arp -a 2>&1",
	"/usr/ucb/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/local/bin/dig com. soa +ti=1 2>&1",
	"/usr/ucb/uptime  2>&1",
	"/usr/ucb/printenv  2>&1",
	"/usr/ucb/netstat -s 2>&1",
	"/usr/local/bin/dig . soa +ti=1 2>&1",
	"/usr/bin/iostat  2>&1",
	"/usr/bin/vm_stat  2>&1",
	"/usr/ucb/w  2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/usr/tmp",
	".",
	"/",
	"/usr/spool",
	"/dev",
	NULL
};

const char *files[] = {
	"/usr/adm/messages",
	"/usr/adm/wtmp",
	"/usr/adm/lastlog",
	NULL
};
#endif /* NEED_PRAND_CONF */

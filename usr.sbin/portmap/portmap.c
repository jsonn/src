/*	$NetBSD: portmap.c,v 1.12.2.1 1999/01/20 07:35:37 cgd Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT(
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)portmap.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: portmap.c,v 1.12.2.1 1999/01/20 07:35:37 cgd Exp $");
#endif
#endif /* not lint */

/*
@(#)portmap.c	2.3 88/08/11 4.0 RPCSRC
static char sccsid[] = "@(#)portmap.c 1.32 87/08/06 Copyr 1984 Sun Micro";
*/

/*
 * portmap.c, Implements the program,version to port number mapping for
 * rpc.
 */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/* who to suid to if -s is given */
#define RUN_AS	"daemon"

#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#ifdef LIBWRAP
# include <tcpd.h>      
#ifndef LIBWRAP_ALLOW_FACILITY
# define LIBWRAP_ALLOW_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_ALLOW_SEVERITY
# define LIBWRAP_ALLOW_SEVERITY LOG_INFO
#endif                  
#ifndef LIBWRAP_DENY_FACILITY
# define LIBWRAP_DENY_FACILITY LOG_AUTH
#endif  
#ifndef LIBWRAP_DENY_SEVERITY
# define LIBWRAP_DENY_SEVERITY LOG_WARNING
#endif
int allow_severity = LIBWRAP_ALLOW_FACILITY|LIBWRAP_ALLOW_SEVERITY;
int deny_severity = LIBWRAP_DENY_FACILITY|LIBWRAP_DENY_SEVERITY;
#endif

struct encap_parms {
	u_int arglen;
	char *args;
};

struct rmtcallargs {
	u_long	rmt_prog;
	u_long	rmt_vers;
	u_long	rmt_port;
	u_long	rmt_proc;
	struct encap_parms rmt_args;
};

static void	callit __P((struct svc_req *, SVCXPRT *));
static struct pmaplist *find_service __P((u_long, u_long, u_long));
int		main __P((int, char *[]));
void		reap __P((int));
void		toggle_verboselog __P((int));
void		reg_service __P((struct svc_req *, SVCXPRT *));
static bool_t	xdr_encap_parms __P((XDR *, struct encap_parms *));
static bool_t	xdr_len_opaque_parms __P((XDR *, struct rmtcallargs *));
static bool_t	xdr_opaque_parms __P((XDR *, struct rmtcallargs *));
static bool_t	xdr_rmtcall_args __P((XDR *, struct rmtcallargs *));
static bool_t	xdr_rmtcall_result __P((XDR *, struct rmtcallargs *));
void		logit __P((int, struct sockaddr_in *, u_long, u_long, char *));
int		check_access __P((struct sockaddr_in *, u_long, u_long));
int		is_loopback __P((struct sockaddr_in *));


struct pmaplist *pmaplist;
int debugging = 0;
int runasdaemon = 0;
int verboselog = 0;

int
main(argc, argv)
	int argc;
	char **argv;
{
	SVCXPRT *xprt;
	int sock, c;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);
	struct pmaplist *pml;

	while ((c = getopt(argc, argv, "dls")) != -1) {
		switch (c) {

		case 'd':
			debugging = 1;
			break;

		case 'l':
			verboselog = 1;
			break;

		case 's':
			runasdaemon = 1;
			break;

		default:
			(void) fprintf(stderr, "usage: %s [-d]\n", argv[0]);
			exit(1);
		}
	}

	if (!debugging && daemon(0, 0)) {
		(void) fprintf(stderr, "portmap: fork: %s", strerror(errno));
		exit(1);
	}

	openlog("portmap", debugging ? LOG_PID | LOG_PERROR : LOG_PID,
	    LOG_DAEMON);

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		syslog(LOG_ERR, "cannot create udp socket: %m");
		exit(1);
	}

	memset((char *)&addr, 0, sizeof addr);
	addr.sin_addr.s_addr = 0;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PMAPPORT);
	if (bind(sock, (struct sockaddr *)&addr, len) != 0) {
		syslog(LOG_ERR, "cannot bind udp: %m");
		exit(1);
	}

	if ((xprt = svcudp_create(sock)) == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "couldn't do udp_create");
		exit(1);
	}
	/* make an entry for ourself */
	pml = (struct pmaplist *)malloc((u_int)sizeof(struct pmaplist));
	pml->pml_next = 0;
	pml->pml_map.pm_prog = PMAPPROG;
	pml->pml_map.pm_vers = PMAPVERS;
	pml->pml_map.pm_prot = IPPROTO_UDP;
	pml->pml_map.pm_port = PMAPPORT;
	pmaplist = pml;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		syslog(LOG_ERR, "cannot create tcp socket: %m");
		exit(1);
	}
	if (bind(sock, (struct sockaddr *)&addr, len) != 0) {
		syslog(LOG_ERR, "cannot bind udp: %m");
		exit(1);
	}
	if ((xprt = svctcp_create(sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE))
	    == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "couldn't do tcp_create");
		exit(1);
	}
	/* make an entry for ourself */
	pml = (struct pmaplist *)malloc((u_int)sizeof(struct pmaplist));
	pml->pml_map.pm_prog = PMAPPROG;
	pml->pml_map.pm_vers = PMAPVERS;
	pml->pml_map.pm_prot = IPPROTO_TCP;
	pml->pml_map.pm_port = PMAPPORT;
	pml->pml_next = pmaplist;
	pmaplist = pml;

	(void)svc_register(xprt, PMAPPROG, PMAPVERS, reg_service, FALSE);

	(void)signal(SIGCHLD, reap);
	(void) signal(SIGUSR1, toggle_verboselog);

	if (runasdaemon) {
		struct passwd *p;

		if((p = getpwnam(RUN_AS)) == NULL) {
			syslog(LOG_ERR, "cannot get uid of daemon: %m");
			exit(1);
		}
		if (setuid(p->pw_uid) == -1) {
			syslog(LOG_ERR, "setuid to daemon failed: %m");
			exit(1);
		}
	}

	svc_run();
	syslog(LOG_ERR, "svc_run returned unexpectedly");
	abort();
}

static struct pmaplist *
find_service(prog, vers, prot)
	u_long prog, vers, prot;
{
	struct pmaplist *hit = NULL;
	struct pmaplist *pml;

	for (pml = pmaplist; pml != NULL; pml = pml->pml_next) {
		if ((pml->pml_map.pm_prog != prog) ||
			(pml->pml_map.pm_prot != prot))
			continue;
		hit = pml;
		if (pml->pml_map.pm_vers == vers)
		    break;
	}
	return (hit);
}

void
reg_service(rqstp, xprt)
	struct svc_req *rqstp;
	SVCXPRT *xprt;
{
	struct pmap reg;
	struct pmaplist *pml, *prevpml, *fnd;
	long ans, port;
	caddr_t t;
	
	if (debugging)
		(void) fprintf(stderr, "server: about to do a switch\n");
	switch (rqstp->rq_proc) {

	case PMAPPROC_NULL:
		/*
		 * Null proc call
		 */
		check_access(svc_getcaller(xprt), rqstp->rq_proc, (u_long) 0);
                if (!svc_sendreply(xprt, xdr_void, (caddr_t)0) && debugging) {
                        abort();
                }
                break;

	case PMAPPROC_SET:
		/*
		 * Set a program,version to port mapping
		 */
		if (!svc_getargs(xprt, xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			if (verboselog)
				logit(allow_severity, svc_getcaller(xprt), rqstp->rq_proc, reg.pm_prog, "");
			if(!is_loopback(svc_getcaller(xprt))) {
				ans = 0;
				goto done;
			}

			/*
			 * check to see if already used
			 * find_service returns a hit even if
			 * the versions don't match, so check for it
			 */
			fnd = find_service(reg.pm_prog, reg.pm_vers,
			    reg.pm_prot);
			if (fnd && fnd->pml_map.pm_vers == reg.pm_vers) {
				if (fnd->pml_map.pm_port == reg.pm_port) {
					ans = 1;
					goto done;
				}
				else {
					ans = 0;
					goto done;
				}
			} else {
				/* 
				 * add to END of list
				 */
				pml = (struct pmaplist *)
				    malloc((u_int)sizeof(struct pmaplist));
				pml->pml_map = reg;
				pml->pml_next = 0;
				if (pmaplist == 0) {
					pmaplist = pml;
				} else {
					for (fnd= pmaplist; fnd->pml_next != 0;
					    fnd = fnd->pml_next);
					fnd->pml_next = pml;
				}
				ans = 1;
			}
		done:
			if ((!svc_sendreply(xprt, xdr_long, (caddr_t)&ans)) &&
			    debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_UNSET:
		/*
		 * Remove a program,version to port mapping.
		 */
		if (!svc_getargs(xprt, xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			ans = 0;
			if (verboselog)
				logit(allow_severity, svc_getcaller(xprt), rqstp->rq_proc, reg.pm_prog, "");
			if(!is_loopback(svc_getcaller(xprt))) {
				goto done;
			}

			for (prevpml = NULL, pml = pmaplist; pml != NULL; ) {
				if ((pml->pml_map.pm_prog != reg.pm_prog) ||
					(pml->pml_map.pm_vers != reg.pm_vers)) {
					/* both pml & prevpml move forwards */
					prevpml = pml;
					pml = pml->pml_next;
					continue;
				}
				/* found it; pml moves forward, prevpml stays */
                                ans = 1;
				t = (caddr_t)pml;
				pml = pml->pml_next;
				if (prevpml == NULL)
					pmaplist = pml;
				else
					prevpml->pml_next = pml;
				free(t);
			}
			if ((!svc_sendreply(xprt, xdr_long, (caddr_t)&ans)) &&
			    debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_GETPORT:
		/*
		 * Lookup the mapping for a program,version and return its port
		 */
		if (!svc_getargs(xprt, xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			if (!check_access(svc_getcaller(xprt), rqstp->rq_proc, reg.pm_prog)) {
				ans = 0;
				/* this is a little bogus -- done is up above, under another case */
				goto done;
			}
			fnd = find_service(reg.pm_prog, reg.pm_vers,
			    reg.pm_prot);
			if (fnd)
				port = fnd->pml_map.pm_port;
			else
				port = 0;
			if ((!svc_sendreply(xprt, xdr_long, (caddr_t)&port)) && debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_DUMP:
		/*
		 * Return the current set of mapped program,version
		 */
		if (!svc_getargs(xprt, xdr_void, NULL))
			svcerr_decode(xprt);
		else {
			 struct pmaplist *p; 
			 if (!check_access(svc_getcaller(xprt), rqstp->rq_proc, (u_long) 0)) {
				p = 0;  /* send empty list */
			 } else {
				p = pmaplist;
			 }

			if ((!svc_sendreply(xprt, xdr_pmaplist, (caddr_t)&p)) && debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_CALLIT:
		/*
		 * Calls a procedure on the local machine.  If the requested
		 * procedure is not registered this procedure does not return
		 * error information!!
		 * This procedure is only supported on rpc/udp and calls via 
		 * rpc/udp.  It passes null authentication parameters.
		 */
		callit(rqstp, xprt);
		break;

	default:
		check_access(svc_getcaller(xprt), rqstp->rq_proc, (u_long) 0);
		svcerr_noproc(xprt);
		break;
	}
}


/*
 * Stuff for the rmtcall service
 */
#define ARGSIZE 9000

static bool_t
xdr_encap_parms(xdrs, epp)
	XDR *xdrs;
	struct encap_parms *epp;
{

	return (xdr_bytes(xdrs, &(epp->args), &(epp->arglen), ARGSIZE));
}

static bool_t
xdr_rmtcall_args(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{

	/* does not get a port number */
	if (xdr_u_long(xdrs, &(cap->rmt_prog)) &&
	    xdr_u_long(xdrs, &(cap->rmt_vers)) &&
	    xdr_u_long(xdrs, &(cap->rmt_proc))) {
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	}
	return (FALSE);
}

static bool_t
xdr_rmtcall_result(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{
	if (xdr_u_long(xdrs, &(cap->rmt_port)))
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	return (FALSE);
}

/*
 * only worries about the struct encap_parms part of struct rmtcallargs.
 * The arglen must already be set!!
 */
static bool_t
xdr_opaque_parms(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{

	return (xdr_opaque(xdrs, cap->rmt_args.args, cap->rmt_args.arglen));
}

/*
 * This routine finds and sets the length of incoming opaque parameters
 * and then calls xdr_opaque_parms.
 */
static bool_t
xdr_len_opaque_parms(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{
	u_int beginpos, lowpos, highpos, currpos, pos;

	beginpos = lowpos = pos = xdr_getpos(xdrs);
	highpos = lowpos + ARGSIZE;
	while ((int)(highpos - lowpos) >= 0) {
		currpos = (lowpos + highpos) / 2;
		if (xdr_setpos(xdrs, currpos)) {
			pos = currpos;
			lowpos = currpos + 1;
		} else {
			highpos = currpos - 1;
		}
	}
	xdr_setpos(xdrs, beginpos);
	cap->rmt_args.arglen = pos - beginpos;
	return (xdr_opaque_parms(xdrs, cap));
}

/*
 * Call a remote procedure service
 * This procedure is very quiet when things go wrong.
 * The proc is written to support broadcast rpc.  In the broadcast case,
 * a machine should shut-up instead of complain, less the requestor be
 * overrun with complaints at the expense of not hearing a valid reply ...
 *
 * This now forks so that the program & process that it calls can call 
 * back to the portmapper.
 */
static void
callit(rqstp, xprt)
	struct svc_req *rqstp;
	SVCXPRT *xprt;
{
	struct rmtcallargs a;
	struct pmaplist *pml;
	u_short port;
	struct sockaddr_in me;
	int pid, so = -1, dontblock = 1;
	CLIENT *client;
	struct authunix_parms *au = (struct authunix_parms *)rqstp->rq_clntcred;
	struct timeval timeout;
	char buf[ARGSIZE];

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	a.rmt_args.args = buf;
	if (!svc_getargs(xprt, xdr_rmtcall_args, (caddr_t)&a))
		return;
	/* host and service access control */
	if (!check_access(svc_getcaller(xprt), rqstp->rq_proc, a.rmt_prog)) 
		return;
	if ((pml = find_service(a.rmt_prog, a.rmt_vers,
	    (u_long)IPPROTO_UDP)) == NULL)
		return;
	/*
	 * fork a child to do the work.  Parent immediately returns.
	 * Child exits upon completion.
	 */
	if ((pid = fork()) != 0) {
		if (pid == -1)
			syslog(LOG_ERR, "CALLIT (prog %lu): fork: %m",
			    a.rmt_prog);
		return;
	}
	port = pml->pml_map.pm_port;
	get_myaddress(&me);
	me.sin_port = htons(port);

	/* Avoid implicit binding to reserved port by clntudp_create() */
	so = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (so == -1)
		exit(1);
	if (ioctl(so, FIONBIO, &dontblock) == -1)
		exit(1);

	client = clntudp_create(&me, a.rmt_prog, a.rmt_vers, timeout, &so);
	if (client != (CLIENT *)NULL) {
		if (rqstp->rq_cred.oa_flavor == AUTH_UNIX) {
			client->cl_auth = authunix_create(au->aup_machname,
			    au->aup_uid, au->aup_gid, au->aup_len, au->aup_gids);
		}
		a.rmt_port = (u_long)port;
		if (clnt_call(client, a.rmt_proc, xdr_opaque_parms, &a,
		    xdr_len_opaque_parms, &a, timeout) == RPC_SUCCESS) {
			svc_sendreply(xprt, xdr_rmtcall_result, (caddr_t)&a);
		}
		AUTH_DESTROY(client->cl_auth);
		clnt_destroy(client);
	}
	(void)close(so);
	exit(0);
}

void
reap(dummy)
	int dummy;
{
	int save_errno = errno;

	while (wait3(NULL, WNOHANG, NULL) > 0)
		;
	errno = save_errno;
}

void
toggle_verboselog(dummy)
	int dummy;
{
	verboselog = !verboselog;
}

int 
check_access(addr, proc, prog)
	struct sockaddr_in *addr; 
	u_long  proc; 
	u_long  prog;
{
#ifdef LIBWRAP
	struct request_info req;
#endif

#ifdef LIBWRAP
	request_init(&req, RQ_DAEMON, "portmap", RQ_CLIENT_SIN, addr, 0);
	sock_methods(&req);
	if(!hosts_access(&req)) {
		logit(deny_severity, addr, proc, prog, ": request from unauthorized host");
		return 0;
	}
#endif
	if (verboselog)
		logit(allow_severity, addr, proc, prog, "");
    	return 1;
}

int
is_loopback(addr)
	struct sockaddr_in *addr;
{
        if (addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
		return 1;
	
	return 0;
}


/* logit - report events of interest via the syslog daemon */
void
logit(severity, addr, procnum, prognum, text)
	int severity;
	struct sockaddr_in *addr;
	u_long procnum;
	u_long prognum; 
	char *text;
{
    char   *procname;
    char    procbuf[4 * sizeof(u_long)];
    char   *progname;
    char    progbuf[4 * sizeof(u_long)];
    struct rpcent *rpc;
    struct proc_map {
        u_long  code;
        char   *proc;
    };  
    struct proc_map *procp;
    static struct proc_map procmap[] = {
        {PMAPPROC_CALLIT, "callit"},
        {PMAPPROC_DUMP, "dump"},
        {PMAPPROC_GETPORT, "getport"}, 
        {PMAPPROC_NULL, "null"},
        {PMAPPROC_SET, "set"},
        {PMAPPROC_UNSET, "unset"},
        {0, 0}
    };  
   
    /*
     * Fork off a process or the portmap daemon might hang while
     * getrpcbynumber() or syslog() does its thing.
     */

    if (fork() == 0) {

        /* Try to map program number to name. */

        if (prognum == 0) {
            progname = "";
        } else if ((rpc = getrpcbynumber((int) prognum))) {
            progname = rpc->r_name;
        } else {
            snprintf(progname = progbuf, sizeof(progbuf), "%lu", prognum);
        }

        /* Try to map procedure number to name. */

        for (procp = procmap; procp->proc && procp->code != procnum; procp++)
             /* void */ ;
        if ((procname = procp->proc) == 0)
            snprintf(procname = procbuf, sizeof(procbuf), "%lu", (u_long) procnum);

        /* Write syslog record. */

        syslog(severity, "connect from %s to %s(%s)%s",
            inet_ntoa(addr->sin_addr), procname, progname, text);
		exit(0);
	}
}

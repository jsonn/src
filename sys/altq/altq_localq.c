/*	$NetBSD: altq_localq.c,v 1.6.12.1 2006/03/18 12:08:18 peter Exp $	*/
/*	$KAME: altq_localq.c,v 1.7 2003/07/10 12:07:48 kjc Exp $	*/
/*
 * a skeleton file for implementing a new queueing discipline.
 * this file is in the public domain.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: altq_localq.c,v 1.6.12.1 2006/03/18 12:08:18 peter Exp $");

#ifdef _KERNEL_OPT
#include "opt_altq.h"
#endif

#ifdef ALTQ_LOCALQ  /* localq is enabled by ALTQ_LOCALQ option in opt_altq.h */

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <netinet/in.h>

#include <altq/altq.h>
#include <altq/altq_conf.h>

#ifdef ALTQ3_COMPAT
/*
 * localq device interface
 */
altqdev_decl(localq);

int
localqopen(dev, flag, fmt, l)
	dev_t dev;
	int flag, fmt;
	struct lwp *l;
{
	/* everything will be done when the queueing scheme is attached. */
	return 0;
}

int
localqclose(dev, flag, fmt, l)
	dev_t dev;
	int flag, fmt;
	struct lwp *l;
{
	int error = 0;

	return error;
}

int
localqioctl(dev, cmd, addr, flag, l)
	dev_t dev;
	ioctlcmd_t cmd;
	caddr_t addr;
	int flag;
	struct lwp *l;
{
	int error = 0;

	return error;
}

#ifdef KLD_MODULE

static struct altqsw localq_sw =
	{"localq", localqopen, localqclose, localqioctl};

ALTQ_MODULE(altq_localq, ALTQT_LOCALQ, &localq_sw);

#endif /* KLD_MODULE */

#endif /* ALTQ3_COMPAT */
#endif /* ALTQ_LOCALQ */

/* $NetBSD: secmodel_bsd44.c,v 1.11.18.2 2010/03/11 15:04:40 yamt Exp $ */
/*-
 * Copyright (c) 2006 Elad Efrat <elad@NetBSD.org>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: secmodel_bsd44.c,v 1.11.18.2 2010/03/11 15:04:40 yamt Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kauth.h>

#include <sys/sysctl.h>
#include <sys/mount.h>

#include <sys/module.h>

#include <secmodel/bsd44/bsd44.h>
#include <secmodel/suser/suser.h>
#include <secmodel/securelevel/securelevel.h>

MODULE(MODULE_CLASS_SECMODEL, secmodel_bsd44, "suser,securelevel");

static struct sysctllog *sysctl_bsd44_log;

void
sysctl_security_bsd44_setup(struct sysctllog **clog)
{
	const struct sysctlnode *rnode;

	sysctl_createv(clog, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "security", NULL,
		       NULL, 0, NULL, 0,
		       CTL_SECURITY, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "models", NULL,
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "bsd44", NULL,
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "name", NULL,
		       NULL, 0, __UNCONST("Traditional NetBSD (derived from 4.4BSD)"), 0,
		       CTL_CREATE, CTL_EOL);
}

void
secmodel_bsd44_init(void)
{
	secmodel_suser_init();
	secmodel_securelevel_init();
}

void
secmodel_bsd44_start(void)
{
	secmodel_suser_start();
	secmodel_securelevel_start();

	/* secmodel_register(); */
}

void
secmodel_bsd44_stop(void)
{
	secmodel_suser_stop();
	secmodel_securelevel_stop();

	/* secmodel_deregister(); */
}

static int
secmodel_bsd44_modcmd(modcmd_t cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
		secmodel_bsd44_init();
		secmodel_bsd44_start();
		sysctl_security_bsd44_setup(&sysctl_bsd44_log);
		break;

	case MODULE_CMD_FINI:
		sysctl_teardown(&sysctl_bsd44_log);
		secmodel_bsd44_stop();
		break;

	default:
		error = ENOTTY;
		break;
	}

	return error;
}


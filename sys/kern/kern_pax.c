/* $NetBSD: kern_pax.c,v 1.3.10.3 2006/12/30 20:50:05 yamt Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Elad Efrat.
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

#include "opt_pax.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/exec_elf.h>
#include <sys/pax.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/fileassoc.h>
#include <sys/syslog.h>
#include <sys/vnode.h>
#include <sys/queue.h>
#include <sys/kauth.h>

#ifdef PAX_MPROTECT
static int pax_mprotect_enabled = 1;
static int pax_mprotect_global = PAX_MPROTECT;

specificdata_key_t pax_mprotect_key;
#endif

#ifdef PAX_SEGVGUARD
#ifndef PAX_SEGVGUARD_EXPIRY
#define	PAX_SEGVGUARD_EXPIRY		(2 * 60)
#endif

#ifndef PAX_SEGVGUARD_SUSPENSION
#define	PAX_SEGVGUARD_SUSPENSION	(10 * 60)
#endif

#ifndef	PAX_SEGVGUARD_MAXCRASHES
#define	PAX_SEGVGUARD_MAXCRASHES	5
#endif

static int pax_segvguard_enabled = 1;
static int pax_segvguard_global = PAX_SEGVGUARD;
static int pax_segvguard_expiry = PAX_SEGVGUARD_EXPIRY;
static int pax_segvguard_suspension = PAX_SEGVGUARD_SUSPENSION;
static int pax_segvguard_maxcrashes = PAX_SEGVGUARD_MAXCRASHES;

static fileassoc_t segvguard_id;
specificdata_key_t pax_segvguard_key;

struct pax_segvguard_uid_entry {
	uid_t sue_uid;
	size_t sue_ncrashes;
	time_t sue_expiry;
	time_t sue_suspended;
	LIST_ENTRY(pax_segvguard_uid_entry) sue_list;
};

struct pax_segvguard_entry {
	LIST_HEAD(, pax_segvguard_uid_entry) segv_uids;
};

static void pax_segvguard_cb(void *);
#endif /* PAX_SEGVGUARD */

/* PaX internal setspecific flags */
#define	PAX_MPROTECT_EXPLICIT_ENABLE	(void *)0x01
#define	PAX_MPROTECT_EXPLICIT_DISABLE	(void *)0x02
#define	PAX_SEGVGUARD_EXPLICIT_ENABLE	(void *)0x03
#define	PAX_SEGVGUARD_EXPLICIT_DISABLE	(void *)0x04

SYSCTL_SETUP(sysctl_security_pax_setup, "sysctl security.pax setup")
{
	const struct sysctlnode *rnode = NULL, *cnode;

        sysctl_createv(clog, 0, NULL, &rnode,
                       CTLFLAG_PERMANENT,
                       CTLTYPE_NODE, "security", NULL,
                       NULL, 0, NULL, 0,
                       CTL_SECURITY, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "pax",
		       SYSCTL_DESCR("PaX (exploit mitigation) features."),
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);

	cnode = rnode;

#ifdef PAX_MPROTECT
	sysctl_createv(clog, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "mprotect",
		       SYSCTL_DESCR("mprotect(2) W^X restrictions."),
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "enabled",
		       SYSCTL_DESCR("Restrictions enabled."),
		       NULL, 0, &pax_mprotect_enabled, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "global",
		       SYSCTL_DESCR("When enabled, unless explicitly "
				    "specified, apply restrictions to "
				    "all processes."),
		       NULL, 0, &pax_mprotect_global, 0,
		       CTL_CREATE, CTL_EOL);
#endif /* PAX_MPROTECT */

	rnode = cnode;

#ifdef PAX_SEGVGUARD
	sysctl_createv(clog, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "segvguard",
		       SYSCTL_DESCR("PaX segvguard."),
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "enabled",
		       SYSCTL_DESCR("segvguard enabled."),
		       NULL, 0, &pax_segvguard_enabled, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "global",
		       SYSCTL_DESCR("segvguard all programs."),
		       NULL, 0, &pax_segvguard_global, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "expiry_timeout",
		       SYSCTL_DESCR("Entry expiry timeout (in seconds)."),
		       NULL, 0, &pax_segvguard_expiry, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "suspend_timeout",
		       SYSCTL_DESCR("Entry suspension timeout (in seconds)."),
		       NULL, 0, &pax_segvguard_suspension, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "max_crashes",
		       SYSCTL_DESCR("Max number of crashes before expiry."),
		       NULL, 0, &pax_segvguard_maxcrashes, 0,
		       CTL_CREATE, CTL_EOL);
#endif /* PAX_SEGVGUARD */
}

/*
 * Initialize PaX.
 */
void
pax_init(void)
{
#ifdef PAX_SEGVGUARD
	int error;
#endif /* PAX_SEGVGUARD */

#ifdef PAX_MPROTECT
	proc_specific_key_create(&pax_mprotect_key, NULL);
#endif /* PAX_MPROTECT */

#ifdef PAX_SEGVGUARD
	error = fileassoc_register("segvguard", pax_segvguard_cb,
	    &segvguard_id);
	if (error) {
		panic("pax_init: segvguard_id: error=%d\n", error);
	}
	proc_specific_key_create(&pax_segvguard_key, NULL);
#endif /* PAX_SEGVGUARD */
}

void
pax_adjust(struct lwp *l, int f)
{
#ifdef PAX_MPROTECT
	if (pax_mprotect_enabled) {
		if (f & PF_PAXMPROTECT)
			proc_setspecific(l->l_proc, pax_mprotect_key,
			    PAX_MPROTECT_EXPLICIT_ENABLE);
		if (f & PF_PAXNOMPROTECT)
			proc_setspecific(l->l_proc, pax_mprotect_key,
			    PAX_MPROTECT_EXPLICIT_DISABLE);
	}
#endif /* PAX_MPROTECT */

#ifdef PAX_SEGVGUARD
	if (pax_segvguard_enabled) {
		if (f & PF_PAXGUARD) {
			proc_setspecific(l->l_proc, pax_segvguard_key,
			    PAX_SEGVGUARD_EXPLICIT_ENABLE);
		}
		if (f & PF_PAXNOGUARD)
			proc_setspecific(l->l_proc, pax_segvguard_key,
			    PAX_SEGVGUARD_EXPLICIT_DISABLE);
	}
#endif /* PAX_SEGVGUARD */
}

#ifdef PAX_MPROTECT
void
pax_mprotect(struct lwp *l, vm_prot_t *prot, vm_prot_t *maxprot)
{
	void *t;

	if (!pax_mprotect_enabled)
		return;

	t = proc_getspecific(l->l_proc, pax_mprotect_key);
	if ((pax_mprotect_global && t == PAX_MPROTECT_EXPLICIT_DISABLE) ||
	    (!pax_mprotect_global && t != PAX_MPROTECT_EXPLICIT_ENABLE))
		return;

	if ((*prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) != VM_PROT_EXECUTE) {
		*prot &= ~VM_PROT_EXECUTE;
		*maxprot &= ~VM_PROT_EXECUTE;
	} else {
		*prot &= ~VM_PROT_WRITE;
		*maxprot &= ~VM_PROT_WRITE;
	}
}
#endif /* PAX_MPROTECT */

#ifdef PAX_SEGVGUARD
static void
pax_segvguard_cb(void *v)
{
	struct pax_segvguard_entry *p;
	struct pax_segvguard_uid_entry *up;

	if (v == NULL)
		return;

	p = v;
	while ((up = LIST_FIRST(&p->segv_uids)) != NULL) {
		LIST_REMOVE(up, sue_list);
		free(up, M_TEMP);
	}

	free(v, M_TEMP);
}

/*
 * Called when a process of image vp generated a segfault.
 */
int
pax_segvguard(struct lwp *l, struct vnode *vp, const char *name,
    boolean_t crashed)
{
	struct pax_segvguard_entry *p;
	struct pax_segvguard_uid_entry *up;
	struct timeval tv;
	uid_t uid;
	void *t;
	boolean_t have_uid;

	if (!pax_segvguard_enabled)
		return (0);

	t = proc_getspecific(l->l_proc, pax_segvguard_key);
	if ((pax_segvguard_global && t == PAX_SEGVGUARD_EXPLICIT_DISABLE) ||
	    (!pax_segvguard_global && t != PAX_SEGVGUARD_EXPLICIT_ENABLE))
		return (0);

	if (vp == NULL)
		return (EFAULT);	

	/* Check if we already monitor the file. */
	p = fileassoc_lookup(vp, segvguard_id);

	/* Fast-path if starting a program we don't know. */
	if (p == NULL && !crashed)
		return (0);

	microtime(&tv);

	/*
	 * If a program we don't know crashed, we need to create a new entry
	 * for it.
	 */
	if (p == NULL) {
		p = malloc(sizeof(*p), M_TEMP, M_WAITOK);
		if (fileassoc_add(vp, segvguard_id, p) != 0) {
			fileassoc_table_add(vp->v_mount, 16);
			fileassoc_add(vp, segvguard_id, p);
		}
		LIST_INIT(&p->segv_uids);

		/*
		 * Initialize a new entry with "crashes so far" of 1.
		 * The expiry time is when we purge the entry if it didn't
		 * reach the limit.
		 */
		up = malloc(sizeof(*up), M_TEMP, M_WAITOK);
		up->sue_uid = kauth_cred_getuid(l->l_cred);
		up->sue_ncrashes = 1;
		up->sue_expiry = tv.tv_sec + pax_segvguard_expiry;
		up->sue_suspended = 0;

		LIST_INSERT_HEAD(&p->segv_uids, up, sue_list);

		return (0);
	}

	/*
	 * A program we "know" either executed or crashed again.
	 * See if it's a culprit we're familiar with.
	 */
	uid = kauth_cred_getuid(l->l_cred);
	have_uid = FALSE;
	LIST_FOREACH(up, &p->segv_uids, sue_list) {
		if (up->sue_uid == uid) {
			have_uid = TRUE;
			break;
		}
	}

	/*
	 * It's someone else. Add an entry for him if we crashed.
	 */
	if (!have_uid) {
		if (crashed) {
			up = malloc(sizeof(*up), M_TEMP, M_WAITOK);
			up->sue_uid = uid;
			up->sue_ncrashes = 1;
			up->sue_expiry = tv.tv_sec + pax_segvguard_expiry;
			up->sue_suspended = 0;

			LIST_INSERT_HEAD(&p->segv_uids, up, sue_list);
		}

		return (0);
	}

	if (crashed) {
		/* Check if timer on previous crashes expired first. */
		if (up->sue_expiry < tv.tv_sec) {
			log(LOG_INFO, "PaX Segvguard: [%s] Suspension"
			    " expired.\n", name ? name : "unknown");

			up->sue_ncrashes = 1;
			up->sue_expiry = tv.tv_sec + pax_segvguard_expiry;
			up->sue_suspended = 0;

			return (0);
		}

		up->sue_ncrashes++;

		if (up->sue_ncrashes >= pax_segvguard_maxcrashes) {
			log(LOG_ALERT, "PaX Segvguard: [%s] Suspending "
			    "execution for %d seconds after %zu crashes.\n",
			    name ? name : "unknown", pax_segvguard_suspension,
			    up->sue_ncrashes);

			/* Suspend this program for a while. */
			up->sue_suspended = tv.tv_sec + pax_segvguard_suspension;
			up->sue_ncrashes = 0;
			up->sue_expiry = 0;
		}
	} else {
		/* Are we supposed to be suspended? */
		if (up->sue_suspended > tv.tv_sec) {
			log(LOG_ALERT, "PaX Segvguard: [%s] Preventing "
			    "execution due to repeated segfaults.\n", name ?
			    name : "unknown");

			return (EPERM);
		}
	}

	return (0);
}
#endif /* PAX_SEGVGUARD */

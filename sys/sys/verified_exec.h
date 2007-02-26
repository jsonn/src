/*	$NetBSD: verified_exec.h,v 1.15.2.3 2007/02/26 09:12:17 yamt Exp $	*/

/*-
 * Copyright (c) 2005, 2006 Elad Efrat <elad@NetBSD.org>
 * Copyright (c) 2005, 2006 Brett Lymn <blymn@NetBSD.org>
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_VERIFIED_EXEC_H_
#define _SYS_VERIFIED_EXEC_H_

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/ioctl.h>

#if defined(_KERNEL) && !defined(HAVE_NBTOOL_CONFIG_H)
#include <sys/malloc.h>
#include <uvm/uvm_extern.h>
#include <uvm/uvm_pglist.h>
#include <uvm/uvm_page.h>
#include <prop/proplib.h>
#endif /* _KERNEL */

/* Flags for a Veriexec entry. These can be OR'd together. */
#define VERIEXEC_DIRECT		0x01 /* Direct execution (exec) */
#define VERIEXEC_INDIRECT	0x02 /* Indirect execution (#!) */
#define VERIEXEC_FILE		0x04 /* Plain file (open) */
#define	VERIEXEC_UNTRUSTED	0x10 /* Untrusted storage */

/* Operations for /dev/veriexec. */
#define VERIEXEC_LOAD		_IOW('X',  0x1, struct plistref)
#define VERIEXEC_TABLESIZE	_IOW('X',  0x2, struct plistref)
#define VERIEXEC_DELETE		_IOW('X',  0x3, struct plistref)
#define VERIEXEC_QUERY		_IOWR('X', 0x4, struct plistref)

/* Veriexec modes (strict levels). */
#define	VERIEXEC_LEARNING	0	/* Learning mode. */
#define	VERIEXEC_IDS		1	/* Intrusion detection mode. */
#define	VERIEXEC_IPS		2	/* Intrusion prevention mode. */
#define	VERIEXEC_LOCKDOWN	3	/* Lockdown mode. */

/* Valid status field values. */
#define FINGERPRINT_NOTEVAL  0  /* fingerprint has not been evaluated */
#define FINGERPRINT_VALID    1  /* fingerprint evaluated and matches list */
#define FINGERPRINT_NOMATCH  2  /* fingerprint evaluated but does not match */

/* Per-page fingerprint status. */
#define	PAGE_FP_NONE	0	/* no per-page fingerprints. */
#define	PAGE_FP_READY	1	/* per-page fingerprints ready for use. */
#define	PAGE_FP_FAIL	2	/* mismatch in per-page fingerprints. */

/*
 * Operations vector for verified exec, this defines the characteristics
 * for the fingerprint type.
 * Function types: init, update, final.
 */
typedef void (*veriexec_fpop_init_t)(void *);
typedef void (*veriexec_fpop_update_t)(void *, u_char *, u_int);
typedef void (*veriexec_fpop_final_t)(u_char *, void *);

#if defined(_KERNEL) && !defined(HAVE_NBTOOL_CONFIG_H)
MALLOC_DECLARE(M_VERIEXEC);

extern int veriexec_strict;

/* Readable values for veriexec_report(). */
#define	REPORT_ALWAYS		0x01	/* Always print */
#define	REPORT_VERBOSE		0x02	/* Print when verbose >= 1 */
#define	REPORT_DEBUG		0x04	/* Print when verbose >= 2 (debug) */
#define	REPORT_PANIC		0x08	/* Call panic() */
#define	REPORT_ALARM		0x10	/* Alarm - also print pid/uid/.. */
#define	REPORT_LOGMASK		(REPORT_ALWAYS|REPORT_VERBOSE|REPORT_DEBUG)

void	veriexecattach(struct device *, struct device *, void *);
int     veriexecopen(dev_t, int, int, struct lwp *);
int     veriexecclose(dev_t, int, int, struct lwp *);
int     veriexecioctl(dev_t, u_long, caddr_t, int, struct lwp *);

void veriexec_init(void);
int veriexec_fpops_add(const char *, size_t, size_t, veriexec_fpop_init_t,
    veriexec_fpop_update_t, veriexec_fpop_final_t);
int veriexec_table_add(struct lwp *, prop_dictionary_t);
int veriexec_file_add(struct lwp *, prop_dictionary_t);
int veriexec_verify(struct lwp *, struct vnode *, const u_char *, int,
    bool *);
#ifdef notyet
int veriexec_page_verify(struct veriexec_file_entry *, struct vm_page *,
    size_t, struct lwp *);
#endif /* notyet */
bool veriexec_lookup(struct vnode *);
int veriexec_file_delete(struct lwp *, struct vnode *);
int veriexec_table_delete(struct lwp *, struct mount *);
int veriexec_convert(struct vnode *, prop_dictionary_t);
void veriexec_report(const u_char *, const u_char *, struct lwp *, int);
void veriexec_purge(struct vnode *);
int veriexec_removechk(struct vnode *, const char *, struct lwp *l);
int veriexec_renamechk(struct vnode *, const char *, struct vnode *,
    const char *, struct lwp *);
int veriexec_unmountchk(struct mount *);
int veriexec_openchk(struct lwp *, struct vnode *, const char *, int);
#endif /* _KERNEL */

#endif /* !_SYS_VERIFIED_EXEC_H_ */

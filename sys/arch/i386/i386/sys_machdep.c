/*	$NetBSD: sys_machdep.c,v 1.70.10.1 2005/01/25 13:01:08 yamt Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_machdep.c,v 1.70.10.1 2005/01/25 13:01:08 yamt Exp $");

#include "opt_compat_netbsd.h"
#include "opt_mtrr.h"
#include "opt_perfctrs.h"
#include "opt_user_ldt.h"
#include "opt_vm86.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/signal.h>
#include <sys/malloc.h>

#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/sysarch.h>
#include <machine/mtrr.h>

#ifdef VM86
#include <machine/vm86.h>
#endif

#ifdef PERFCTRS
#include <machine/pmc.h>
#endif

extern struct vm_map *kernel_map;

int i386_iopl(struct lwp *, void *, register_t *);
int i386_get_ioperm(struct lwp *, void *, register_t *);
int i386_set_ioperm(struct lwp *, void *, register_t *);
int i386_get_mtrr(struct lwp *, void *, register_t *);
int i386_set_mtrr(struct lwp *, void *, register_t *);

#ifdef USER_LDT

#ifdef LDT_DEBUG
static void i386_print_ldt(int, const struct segment_descriptor *);

static void
i386_print_ldt(i, d)
	int  i;
	const struct segment_descriptor *d;
{
	printf("[%d] lolimit=0x%x, lobase=0x%x, type=%u, dpl=%u, p=%u, "
	    "hilimit=0x%x, xx=%x, def32=%u, gran=%u, hibase=0x%x\n",
	    i, d->sd_lolimit, d->sd_lobase, d->sd_type, d->sd_dpl, d->sd_p,
	    d->sd_hilimit, d->sd_xx, d->sd_def32, d->sd_gran, d->sd_hibase);
}
#endif

int
i386_get_ldt(l, args, retval)
	struct lwp *l;
	void *args;
	register_t *retval;
{
	int error;
	struct proc *p = l->l_proc;
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	int nldt, num;
	union descriptor *lp, *cp;
	struct i386_get_ldt_args ua;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

#ifdef	LDT_DEBUG
	printf("i386_get_ldt: start=%d num=%d descs=%p\n", ua.start,
	    ua.num, ua.desc);
#endif

	if (ua.start < 0 || ua.num < 0 || ua.start > 8192 || ua.num > 8192 ||
	    ua.start + ua.num > 8192)
		return (EINVAL);

	cp = malloc(ua.num * sizeof(union descriptor), M_TEMP, M_WAITOK);
	if (cp == NULL)
		return ENOMEM;

	simple_lock(&pmap->pm_lock);

	if (pmap->pm_flags & PMF_USER_LDT) {
		nldt = pmap->pm_ldt_len;
		lp = pmap->pm_ldt;
	} else {
		nldt = NLDT;
		lp = ldt;
	}

	if (ua.start > nldt) {
		simple_unlock(&pmap->pm_lock);
		free(cp, M_TEMP);
		return (EINVAL);
	}

	lp += ua.start;
	num = min(ua.num, nldt - ua.start);
#ifdef LDT_DEBUG
	{
		int i;
		for (i = 0; i < num; i++)
			i386_print_ldt(i, &lp[i].sd);
	}
#endif

	memcpy(cp, lp, num * sizeof(union descriptor));
	simple_unlock(&pmap->pm_lock);

	error = copyout(cp, ua.desc, num * sizeof(union descriptor));
	if (error == 0)
		*retval = num;

	free(cp, M_TEMP);
	return (error);
}

int
i386_set_ldt(l, args, retval)
	struct lwp *l;
	void *args;
	register_t *retval;
{
	int error, i, n;
	struct proc *p = l->l_proc;
	struct pcb *pcb = &l->l_addr->u_pcb;
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	struct i386_set_ldt_args ua;
	union descriptor *descv;
	size_t old_len, new_len, ldt_len;
	union descriptor *old_ldt, *new_ldt;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	if (ua.start < 0 || ua.num < 0 || ua.start > 8192 || ua.num > 8192 ||
	    ua.start + ua.num > 8192)
		return (EINVAL);

	descv = malloc(sizeof (*descv) * ua.num, M_TEMP, M_NOWAIT);
	if (descv == NULL)
		return (ENOMEM);

	if ((error = copyin(ua.desc, descv, sizeof (*descv) * ua.num)) != 0)
		goto out;

	/* Check descriptors for access violations. */
	for (i = 0; i < ua.num; i++) {
		union descriptor *desc = &descv[i];

		switch (desc->sd.sd_type) {
		case SDT_SYSNULL:
			desc->sd.sd_p = 0;
			break;
		case SDT_SYS286CGT:
		case SDT_SYS386CGT:
			/*
			 * Only allow call gates targeting a segment
			 * in the LDT or a user segment in the fixed
			 * part of the gdt.  Segments in the LDT are
			 * constrained (below) to be user segments.
			 */
			if (desc->gd.gd_p != 0 &&
			    !ISLDT(desc->gd.gd_selector) &&
			    ((IDXSEL(desc->gd.gd_selector) >= NGDT) ||
			     (gdt[IDXSEL(desc->gd.gd_selector)].sd.sd_dpl !=
				 SEL_UPL))) {
				error = EACCES;
				goto out;
			}
			break;
		case SDT_MEMEC:
		case SDT_MEMEAC:
		case SDT_MEMERC:
		case SDT_MEMERAC:
			/* Must be "present" if executable and conforming. */
			if (desc->sd.sd_p == 0) {
				error = EACCES;
				goto out;
			}
			break;
		case SDT_MEMRO:
		case SDT_MEMROA:
		case SDT_MEMRW:
		case SDT_MEMRWA:
		case SDT_MEMROD:
		case SDT_MEMRODA:
		case SDT_MEMRWD:
		case SDT_MEMRWDA:
		case SDT_MEME:
		case SDT_MEMEA:
		case SDT_MEMER:
		case SDT_MEMERA:
			break;
		default:
			/*
			 * Make sure that unknown descriptor types are
			 * not marked present.
			 */
			if (desc->sd.sd_p != 0) {
				error = EACCES;
				goto out;
			}
			break;
		}

		if (desc->sd.sd_p != 0) {
			/* Only user (ring-3) descriptors may be present. */
			if (desc->sd.sd_dpl != SEL_UPL) {
				error = EACCES;
				goto out;
			}
		}
	}

	/* allocate user ldt */
	simple_lock(&pmap->pm_lock);
	if (pmap->pm_ldt == 0 || (ua.start + ua.num) > pmap->pm_ldt_len) {
		if (pmap->pm_flags & PMF_USER_LDT)
			ldt_len = pmap->pm_ldt_len;
		else
			ldt_len = 512;
		while ((ua.start + ua.num) > ldt_len)
			ldt_len *= 2;
		new_len = ldt_len * sizeof(union descriptor);

		simple_unlock(&pmap->pm_lock);
		new_ldt = (union descriptor *)uvm_km_alloc(kernel_map,
		    new_len, 0, UVM_KMF_WIRED);
		simple_lock(&pmap->pm_lock);

		if (pmap->pm_ldt != NULL && ldt_len <= pmap->pm_ldt_len) {
			/*
			 * Another thread (re)allocated the LDT to
			 * sufficient size while we were blocked in
			 * uvm_km_alloc. Oh well. The new entries
			 * will quite probably not be right, but
			 * hey.. not our problem if user applications
			 * have race conditions like that.
			 */
			uvm_km_free(kernel_map, (vaddr_t)new_ldt, new_len,
			    UVM_KMF_WIRED);
			goto copy;
		}

		old_ldt = pmap->pm_ldt;

		if (old_ldt != NULL) {
			old_len = pmap->pm_ldt_len * sizeof(union descriptor);
		} else {
			old_len = NLDT * sizeof(union descriptor);
			old_ldt = ldt;
		}

		memcpy(new_ldt, old_ldt, old_len);
		memset((caddr_t)new_ldt + old_len, 0, new_len - old_len);

		if (old_ldt != ldt)
			uvm_km_free(kernel_map, (vaddr_t)old_ldt, old_len,
			    UVM_KMF_WIRED);

		pmap->pm_ldt = new_ldt;
		pmap->pm_ldt_len = ldt_len;

		if (pmap->pm_flags & PMF_USER_LDT)
			ldt_free(pmap);
		else
			pmap->pm_flags |= PMF_USER_LDT;
		ldt_alloc(pmap, new_ldt, new_len);
		pcb->pcb_ldt_sel = pmap->pm_ldt_sel;
		if (pcb == curpcb)
			lldt(pcb->pcb_ldt_sel);

	}
copy:
	/* Now actually replace the descriptors. */
	for (i = 0, n = ua.start; i < ua.num; i++, n++)
		pmap->pm_ldt[n] = descv[i];

	simple_unlock(&pmap->pm_lock);

	*retval = ua.start;

out:
	free(descv, M_TEMP);
	return (error);
}
#endif	/* USER_LDT */

int
i386_iopl(l, args, retval)
	struct lwp *l;
	void *args;
	register_t *retval;
{
	int error;
	struct proc *p = l->l_proc;
	struct trapframe *tf = l->l_md.md_regs;
	struct i386_iopl_args ua;

	if (securelevel > 1)
		return EPERM;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return error;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return error;

	if (ua.iopl)
		tf->tf_eflags |= PSL_IOPL;
	else
		tf->tf_eflags &= ~PSL_IOPL;

	return 0;
}

int
i386_get_ioperm(l, args, retval)
	struct lwp *l;
	void *args;
	register_t *retval;
{
	int error;
	struct pcb *pcb = &l->l_addr->u_pcb;
	struct i386_get_ioperm_args ua;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	return copyout(pcb->pcb_iomap, ua.iomap, sizeof(pcb->pcb_iomap));
}

int
i386_set_ioperm(l, args, retval)
	struct lwp *l;
	void *args;
	register_t *retval;
{
	int error;
	struct proc *p = l->l_proc;
	struct pcb *pcb = &l->l_addr->u_pcb;
	struct i386_set_ioperm_args ua;

	if (securelevel > 1)
		return EPERM;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return error;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	return copyin(ua.iomap, pcb->pcb_iomap, sizeof(pcb->pcb_iomap));
}

#ifdef MTRR
int
i386_get_mtrr(struct lwp *l, void *args, register_t *retval)
{
	struct i386_get_mtrr_args ua;
	int error, n;
	struct proc *p = l->l_proc;

	if (mtrr_funcs == NULL)
		return ENOSYS;

	error = copyin(args, &ua, sizeof ua);
	if (error != 0)
		return error;

	error = copyin(ua.n, &n, sizeof n);
	if (error != 0)
		return error;

	error = mtrr_get(ua.mtrrp, &n, p, MTRR_GETSET_USER);

	copyout(&n, ua.n, sizeof (int));

	return error;
}

int
i386_set_mtrr(struct lwp *l, void *args, register_t *retval)
{
	int error, n;
	struct i386_set_mtrr_args ua;
	struct proc *p = l->l_proc;

	if (mtrr_funcs == NULL)
		return ENOSYS;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error != 0)
		return error;

	error = copyin(args, &ua, sizeof ua);
	if (error != 0)
		return error;

	error = copyin(ua.n, &n, sizeof n);
	if (error != 0)
		return error;

	error = mtrr_set(ua.mtrrp, &n, p, MTRR_GETSET_USER);
	if (n != 0)
		mtrr_commit();

	copyout(&n, ua.n, sizeof n);

	return error;
}
#endif

int
sys_sysarch(struct lwp *l, void *v, register_t *retval)
{
	struct sys_sysarch_args /* {
		syscallarg(int) op;
		syscallarg(void *) parms;
	} */ *uap = v;
	int error = 0;

	switch(SCARG(uap, op)) {
#ifdef	USER_LDT
	case I386_GET_LDT: 
		error = i386_get_ldt(l, SCARG(uap, parms), retval);
		break;

	case I386_SET_LDT: 
		error = i386_set_ldt(l, SCARG(uap, parms), retval);
		break;
#endif

	case I386_IOPL: 
		error = i386_iopl(l, SCARG(uap, parms), retval);
		break;

	case I386_GET_IOPERM: 
		error = i386_get_ioperm(l, SCARG(uap, parms), retval);
		break;

	case I386_SET_IOPERM: 
		error = i386_set_ioperm(l, SCARG(uap, parms), retval);
		break;

#ifdef VM86
	case I386_VM86:
		error = i386_vm86(l, SCARG(uap, parms), retval);
		break;
#ifdef COMPAT_16
	case I386_OLD_VM86:
		error = compat_16_i386_vm86(l, SCARG(uap, parms), retval);
		break;
#endif
#endif
#ifdef MTRR
	case I386_GET_MTRR:
		error = i386_get_mtrr(l, SCARG(uap, parms), retval);
		break;
	case I386_SET_MTRR:
		error = i386_set_mtrr(l, SCARG(uap, parms), retval);
		break;
#endif
#ifdef PERFCTRS
	case I386_PMC_INFO:
		error = pmc_info(l, SCARG(uap, parms), retval);
		break;

	case I386_PMC_STARTSTOP:
		error = pmc_startstop(l, SCARG(uap, parms), retval);
		break;

	case I386_PMC_READ:
		error = pmc_read(l, SCARG(uap, parms), retval);
		break;
#endif

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

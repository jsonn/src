/* $NetBSD: privcmd.c,v 1.1.8.2 2005/02/12 22:30:24 bouyer Exp $ */

/*
 *
 * Copyright (c) 2004 Christian Limpach.
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
 *      This product includes software developed by Christian Limpach.
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


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: privcmd.c,v 1.1.8.2 2005/02/12 22:30:24 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/kernfs/kernfs.h>

#include <machine/kernfs_machdep.h>
#include <machine/xenio.h>

#define	PRIVCMD_MODE	(S_IRUSR)


static int
privcmd_ioctl(void *v)
{
	struct vop_ioctl_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		u_long a_command;
		void *a_data;
		int a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	int error = 0;

	switch (ap->a_command) {
	case IOCTL_PRIVCMD_HYPERCALL:
		__asm__ __volatile__ (
			"pushl %%ebx; pushl %%ecx; pushl %%edx;"
			"pushl %%esi; pushl %%edi; "
			"movl  4(%%eax),%%ebx ;"
			"movl  8(%%eax),%%ecx ;"
			"movl 12(%%eax),%%edx ;"
			"movl 16(%%eax),%%esi ;"
			"movl 20(%%eax),%%edi ;"
			"movl   (%%eax),%%eax ;"
			TRAP_INSTR "; "
			"popl %%edi; popl %%esi; popl %%edx;"
			"popl %%ecx; popl %%ebx"
			: "=a" (error) : "0" (ap->a_data) : "memory" );
		error = -error;
		break;
	case IOCTL_PRIVCMD_INITDOMAIN_EVTCHN:
		{
		extern int initdom_ctrlif_domcontroller_port;
		error = initdom_ctrlif_domcontroller_port;
		}
		break;
	case IOCTL_PRIVCMD_MMAP:
	{
		int i, j, error;
		privcmd_mmap_t *mcmd = ap->a_data;
		privcmd_mmap_entry_t mentry;
		vaddr_t va;
		u_long ma;
		//printf("IOCTL_PRIVCMD_MMAP: %d entries\n", mcmd->num);

		pmap_t pmap = vm_map_pmap(&ap->a_p->p_vmspace->vm_map);
		for (i = 0; i < mcmd->num; i++) {
			error = copyin(mcmd->entry, &mentry, sizeof(mentry));
			if (error)
				return error;
			//printf("entry %d va 0x%lx npages %lu mfm 0x%lx\n", i, mentry.va, mentry.npages, mentry.mfn);
			if (mentry.va > VM_MAXUSER_ADDRESS)
				return EINVAL;
#if 0
			if (mentry.va + (mentry.npages << PGSHIFT) >
			    mrentry->vm_end)
				return EINVAL;
#endif
			va = mentry.va & ~PAGE_MASK;
			ma = mentry.mfn <<  PGSHIFT; /* XXX ??? */
			for (j = 0; j < mentry.npages; j++) {
				//printf("remap va 0x%lx to 0x%lx\n", va, ma);
#if 0
				if (!pmap_extract(pmap, va, NULL))
					return EINVAL; /* XXX */
#endif
				if ((error = pmap_remap_pages(pmap, va, ma, 1,
				    PMAP_WIRED | PMAP_CANFAIL, mcmd->dom)))
					return error;
				va += PAGE_SIZE;
				ma += PAGE_SIZE;
			}
		}
		break;
	}
	case IOCTL_PRIVCMD_MMAPBATCH:
		/* XXX */
		printf("IOCTL_PRIVCMD_MMAPBATCH\n");
		error = EOPNOTSUPP;
		break;
	case IOCTL_PRIVCMD_GET_MACH2PHYS_START_MFN:
		{
		unsigned long *mfn_start = ap->a_data;
		*mfn_start = HYPERVISOR_shared_info->arch.mfn_to_pfn_start;
		error = 0;
		}
		break;
	default:
		error = EINVAL;
	}
	
	return error;
}

static const struct kernfs_fileop privcmd_fileops[] = {
  { .kf_fileop = KERNFS_FILEOP_IOCTL, .kf_vop = privcmd_ioctl },
};

void
xenprivcmd_init()
{
	kernfs_entry_t *dkt;
	kfstype kfst;

	if ((xen_start_info.flags & SIF_PRIVILEGED) == 0)
		return;

	kfst = KERNFS_ALLOCTYPE(privcmd_fileops);

	KERNFS_ALLOCENTRY(dkt, M_TEMP, M_WAITOK);
	KERNFS_INITENTRY(dkt, DT_REG, "privcmd", NULL, kfst, VREG,
	    PRIVCMD_MODE);
	kernfs_addentry(kernxen_pkt, dkt);
}

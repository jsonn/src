/*	$NetBSD: mem.c,v 1.21.2.1 2001/09/13 01:13:18 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *
 *	@(#)mem.c	8.3 (Berkeley) 1/12/94
 */

/*
 * Memory special file
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/pmap.h>

#include "nvr.h"

#define mmread  mmrw
#define mmwrite mmrw
cdev_decl(mm);

extern u_int lowram;
static caddr_t devzeropage;

/*ARGSUSED*/
int
mmopen(dev, flag, mode, p)
	dev_t 		dev;
	int		flag, mode;
	struct proc	*p;
{

	return (0);
}

/*ARGSUSED*/
int
mmclose(dev, flag, mode, p)
	dev_t 		dev;
	int		flag, mode;
	struct proc	*p;
{

	return (0);
}

/*ARGSUSED*/
int
mmrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	vsize_t		o, v;
	int		c;
	struct iovec	*iov;
	struct memseg	*ms;
	int		error = 0;
	static int	physlock;
	vm_prot_t	prot;

	if (minor(dev) == 0) {
		/* lock against other uses of shared vmmap */
		while (physlock > 0) {
			physlock++;
			error = tsleep((caddr_t)&physlock, PZERO | PCATCH,
			    "mmrw", 0);
			if (error)
				return (error);
		}
		physlock = 1;
	}
	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}
		switch (minor(dev)) {

		case 0: /* minor device 0 is physical memory */
			v = uio->uio_offset;

			/*
			 * Limit access to RAM actually present
			 */
			for (ms = boot_segs; ms->start != ms->end; ms++)
				if ((v >= ms->start) && (v < ms->end))
					break;
			if (ms->start == ms->end) {
				error = EFAULT;
				goto unlock;
			}

			prot = uio->uio_rw == UIO_READ ? VM_PROT_READ :
			    VM_PROT_WRITE;
			pmap_enter(pmap_kernel(), (vaddr_t)vmmap,
			    trunc_page(v), prot, prot|PMAP_WIRED);
			pmap_update(pmap_kernel());
			o = uio->uio_offset & PGOFSET;
			c = min(uio->uio_resid, (int)(NBPG - o));
			error = uiomove((caddr_t)vmmap + o, c, uio);
			pmap_remove(pmap_kernel(), (vaddr_t)vmmap,
			    (vaddr_t)vmmap + NBPG);
			pmap_update(pmap_kernel());
			break;

		case 1: /* minor device 1 is kernel memory */
			v = uio->uio_offset;
			c = min(iov->iov_len, MAXPHYS);
			if (!uvm_kernacc((caddr_t)v, c,
			    uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
				return (EFAULT);
			error = uiomove((caddr_t)v, c, uio);
			break;

		case 2: /* minor device 2 is EOF/RATHOLE */
			if (uio->uio_rw == UIO_WRITE)
				uio->uio_resid = 0;
			return (0);

		case 11: /* minor device 11 (/dev/nvram) */
#if NNVR > 0
			error = nvram_uio(uio);
			return (error);
#else
			return (ENXIO);
#endif

		case 12: /* minor device 12 (/dev/zero) */
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (devzeropage == NULL) {
				devzeropage = (caddr_t)
				    malloc(NBPG, M_TEMP, M_WAITOK);
				bzero(devzeropage, NBPG);
			}
			c = min(iov->iov_len, NBPG);
			error = uiomove(devzeropage, c, uio);
			break;

		default:
			return (ENXIO);
		}
	}
	if (minor(dev) == 0) {
unlock:
		if (physlock > 1)
			wakeup((caddr_t)&physlock);
		physlock = 0;
	}
	return (error);
}

paddr_t
mmmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{

	return (-1);
}

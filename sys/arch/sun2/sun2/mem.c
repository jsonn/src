/*	$NetBSD: mem.c,v 1.10.2.4 2005/01/24 08:34:34 skrll Exp $	*/

/*
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)mem.c	8.3 (Berkeley) 1/12/94
 */

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
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
 *	from: @(#)mem.c	8.3 (Berkeley) 1/12/94
 */

/*
 * Memory special file
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mem.c,v 1.10.2.4 2005/01/24 08:34:34 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/leds.h>
#include <machine/mon.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include <sun2/sun2/machdep.h>

#define DEV_VME16D16	5	/* minor device 5 is /dev/vme16d16 */
#define DEV_VME24D16	6	/* minor device 6 is /dev/vme24d16 */
#define DEV_EEPROM	11 	/* minor device 11 is eeprom */
#define DEV_LEDS	13 	/* minor device 13 is leds */

static int promacc(caddr_t, int, int);
static caddr_t devzeropage;

dev_type_read(mmrw);
dev_type_ioctl(mmioctl);
dev_type_mmap(mmmmap);

const struct cdevsw mem_cdevsw = {
	nullopen, nullclose, mmrw, mmrw, mmioctl,
	nostop, notty, nopoll, mmmmap, nokqfilter,
};

/*ARGSUSED*/
int 
mmrw(dev_t dev, struct uio *uio, int flags)
{
	struct iovec *iov;
	vaddr_t o, v;
	int c, rw;
	int error = 0;
	static int physlock;
	vm_prot_t prot;

	if (minor(dev) == DEV_MEM) {
		if (vmmap == 0)
			return (EIO);
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

		case DEV_MEM:
			v = uio->uio_offset;
			/* allow reads only in RAM */
			if (v >= avail_end) {
				error = EFAULT;
				goto unlock;
			}
			/*
			 * If the offset (physical address) is outside the
			 * region of physical memory that is "managed" by
			 * the pmap system, then we are not allowed to
			 * call pmap_enter with that physical address.
			 * Physical 0x0 -> physical 0x2000 is unmapped.
			 * Everything from 0x2000 to avail_start is mapped
			 * linearly with physical 0x2000 at virtual 0x2000,
			 * so redirect the access to /dev/kmem instead.
			 * This is a better alternative than hacking the
			 * pmap to deal with requests on unmanaged memory.
			 * Also note: unlock done at end of function.
			 */
			if (v < 0x2000) {
				error = EFAULT;
				goto unlock;
			}
			if (v < avail_start) {
				goto use_kmem;
			}
			/* Temporarily map the memory at vmmap. */
			prot = uio->uio_rw == UIO_READ ? VM_PROT_READ :
			    VM_PROT_WRITE;
			pmap_enter(pmap_kernel(), (vaddr_t)vmmap,
			    trunc_page(v), prot, prot|PMAP_WIRED);
			pmap_update(pmap_kernel());
			o = v & PGOFSET;
			c = min(uio->uio_resid, (int)(PAGE_SIZE - o));
			error = uiomove((caddr_t)vmmap + o, c, uio);
			pmap_remove(pmap_kernel(), (vaddr_t)vmmap,
			    (vaddr_t)vmmap + PAGE_SIZE);
			pmap_update(pmap_kernel());
			break;

		case DEV_KMEM:
			v = uio->uio_offset;
		use_kmem:
			/*
			 * Watch out!  You might assume it is OK to copy
			 * up to MAXPHYS bytes here, but that is wrong.
			 * The next page might NOT be part of the range:
			 *   	(KERNBASE..(KERNBASE+avail_start))
			 * which is asked for here via the goto in the
			 * /dev/mem case above.  The consequence is that
			 * we copy one page at a time.  No big deal, as
			 * most requests are less than one page anyway.
			 */
			o = v & PGOFSET;
			c = min(uio->uio_resid, (int)(PAGE_SIZE - o));
			rw = (uio->uio_rw == UIO_READ) ? B_READ : B_WRITE;
			if (!(uvm_kernacc((caddr_t)v, c, rw) ||
			      promacc((caddr_t)v, c, rw)))
			{
				error = EFAULT;
				/* Note: case 0 can get here, so must unlock! */
				goto unlock;
			}
			error = uiomove((caddr_t)v, c, uio);
			break;

		case DEV_NULL:
			if (uio->uio_rw == UIO_WRITE)
				uio->uio_resid = 0;
			return (0);

		case DEV_ZERO:
			/* Write to /dev/zero is ignored. */
			if (uio->uio_rw == UIO_WRITE) {
				uio->uio_resid = 0;
				return (0);
			}
			/*
			 * On the first call, allocate and zero a page
			 * of memory for use with /dev/zero.
			 */
			if (devzeropage == NULL) {
				devzeropage = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
				memset(devzeropage, 0, PAGE_SIZE);
			}
			c = min(iov->iov_len, PAGE_SIZE);
			error = uiomove(devzeropage, c, uio);
			break;

		case DEV_LEDS:
			error = leds_uio(uio);
			/* Yes, return (not break) so EOF works. */
			return (error);

		default:
			return (ENXIO);
		}
	}

	/*
	 * Note the different location of this label, compared with
	 * other ports.  This is because the /dev/mem to /dev/kmem
	 * redirection above jumps here on error to do its unlock.
	 */
unlock:
	if (minor(dev) == DEV_MEM) {
		if (physlock > 1)
			wakeup((caddr_t)&physlock);
		physlock = 0;
	}
	return (error);
}

paddr_t 
mmmmap(dev_t dev, off_t off, int prot)
{
	/*
	 * Check address validity.
	 */
	if (off & PGOFSET)
		return (-1);

	switch (minor(dev)) {

	case DEV_MEM:
		/* Allow access only in "managed" RAM. */
		if (off < avail_start || off >= avail_end)
			break;
		return (off);

	case DEV_VME16D16:
		if (off & 0xffff0000)
			break;
		off |= 0xff0000;
		/* fall through */
	case DEV_VME24D16:
		if (off & 0xff000000)
			break;
		return (off | (off & 0x800000 ? PMAP_VME8: PMAP_VME0));

	}

	return (-1);
}


/*
 * Just like uvm_kernacc(), but for the PROM mappings.
 * Return non-zero if access at VA is allowed.
 */
static int 
promacc(caddr_t va, int len, int rw)
{
	vaddr_t sva, eva;

	sva = (vaddr_t)va;
	eva = (vaddr_t)va + len;

	/* Test for the most common case first. */
	if (sva < SUN2_PROM_BASE)
		return (0);

	/* Read in the PROM itself is OK. */
	if ((rw == B_READ) && (eva <= SUN2_MONEND))
		return (1);

	/* otherwise, not OK to touch */
	return (0);
}

/*	$NetBSD: db_memrw.c,v 1.2.12.1 2000/11/20 20:31:15 bouyer Exp $	*/
/*	$OpenBSD: db_memrw.c,v 1.2 1996/12/28 06:21:52 rahnds Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

/*
 * Interface to the debugger for virtual memory read/write.
 * This is a simple version for kernels with writable text.
 * For an example of read-only kernel text, see the file:
 * sys/arch/sun3/sun3/db_memrw.c
 *
 * ALERT!  If you want to access device registers with a
 * specific size, then the read/write functions have to
 * make sure to do the correct sized pointer access.
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>

#include <ddb/db_access.h>

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vaddr_t		addr;
	register size_t	size;
	register char	*data;
{
	register char	*src = (char*)addr;

	if (size == 4) {
		*((int*)data) = *((int*)src);
		return;
	}

	if (size == 2) {
		*((short*)data) = *((short*)src);
		return;
	}

	while (size > 0) {
		--size;
		*data++ = *src++;
	}
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vaddr_t		addr;
	register size_t	size;
	register char	*data;
{
	register char	*dst = (char *)addr;

	if (size == 4) {
		*((int*)dst) = *((int*)data);
		return;
	}

	if (size == 2) {
		*((short*)dst) = *((short*)data);
		return;
	}

	while (size > 0) {
		--size;
		*dst++ = *data++;
	}
}


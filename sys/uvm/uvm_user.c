/*	$NetBSD: uvm_user.c,v 1.4.2.1 1998/07/30 14:04:16 eeh Exp $	*/

/*
 * XXXCDC: "ROUGH DRAFT" QUALITY UVM PRE-RELEASE FILE!   
 *	   >>>USE AT YOUR OWN RISK, WORK IS NOT FINISHED<<<
 */
/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
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
 *
 * from: Id: uvm_user.c,v 1.1.2.1 1997/08/14 19:10:41 chuck Exp
 */

/*
 * uvm_user.c: high level uvm_allocate/uvm_deallocate interface into vm.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <uvm/uvm.h>

/*
 * uvm_deallocate: deallocate memory (unmap)
 */

int
uvm_deallocate(map, start, size)
	vm_map_t map;
	vaddr_t start;
	vsize_t size;
{

	if (map == NULL)
		panic("uvm_deallocate with null map");

	if (size == (vaddr_t) 0)
		return (KERN_SUCCESS);

	return(uvm_unmap(map, trunc_page(start), round_page(start+size), 0));

}

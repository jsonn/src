/* 
 * Copyright (c) 1987 Carnegie-Mellon University
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * Changed for the VAX port. /IC
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
 *	from: @(#)pmap.h	7.6 (Berkeley) 5/10/91
 *	$Id: pmap.h,v 1.2.2.2 1994/08/16 23:41:57 ragge Exp $
 */


#ifndef	PMAP_H
#define	PMAP_H

#include "vm/vm_param.h"
#include "vm/lock.h"
/* #include "vm/vm_statistics.h" */

#define VAX_PAGE_SIZE	NBPG
#define VAX_SEG_SIZE	NBSEG

/*
 *  Pmap structure
 */

/* XXX Should reside in #include "include/pmap.h" */
typedef struct pmap {
  simple_lock_data_t       lock;        /* lock on pmap        */
  int                      ref_count;   /* reference count     */
  struct pmap_statistics   stats;       /* statistics          */
  struct pmap             *next;        /* list for free pmaps */
  struct pte	          *pm_ptab;	/* KVA of page table   */
} *pmap_t;

#if 0
struct pmap {
  struct pte	       *pm_ptab;	/* KVA of page table    */
  short			pm_count;	/* pmap reference count */
  simple_lock_data_t	pm_lock;	/* lock on pmap         */
};
#endif

/*typedef struct pmap    *pmap_t;*/

extern pmap_t		kernel_pmap;

/*
 * Macros for speed
 */

#define PMAP_ACTIVATE(pmapp, pcbp, iscurproc)                  \
  if ((pmapp) != NULL && (pmapp)->pm_stchanged) {              \
    (pcbp)->pcb_ustp =                                         \
        vax_btop(pmap_extract(kernel_pmap, (pmapp)->pm_stab)); \
    if (iscurproc)                                             \
      loadustp((pcbp)->pcb_ustp);                              \
    (pmapp)->pm_stchanged = FALSE;                             \
  }

#define PMAP_DEACTIVATE(pmapp, pcbp)

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */

typedef struct pv_entry {
  struct pv_entry	*pv_next;	/* next pv_entry */
  struct pmap	        *pv_pmap;/* if not NULL, pmap where mapping lies */
  vm_offset_t	         pv_va;		/* virtual address for mapping */
  int		         pv_flags;	/* flags */
} *pv_entry_t;

#define	PV_CI		0x01	        /* all entries must be cache inhibited */
#define PV_PTPAGE	0x02	        /* entry maps a page table page */

#ifdef	KERNEL
pv_entry_t	pv_table;		/* array of entries, 
					   one per LOGICAL page */

#define pa_index(pa)	                atop(pa)
#define pa_to_pvh(pa)	                (&pv_table[atop(pa)])

#define	pmap_kernel()			(kernel_pmap)
/* #define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count) */

extern	struct pte *Sysmap;
extern	char *vmmap;			/* map for mem, dumps, etc. */
#endif	KERNEL

#endif PMAP_H

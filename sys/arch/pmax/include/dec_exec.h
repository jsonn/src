/*	$NetBSD: dec_exec.h,v 1.4.2.1 1999/02/24 02:11:10 nisimura Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)dec_exec.h	8.1 (Berkeley) 6/10/93
 */


/*
 * Portions of this file are subject to the following copyright notice:
 *
 * Copyright (C) 1989 Digital Equipment Corporation.
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.  
 * Digital Equipment Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

/*
 * /sprite/src/kernel/proc/ds3100.md/RCS/procMach.h,v 9.3 90/02/20 15:35:50
 * shirriff Exp  SPRITE (Berkeley)
 */

/* Description of the COFF section. */
struct coff_exec {
#define	COFF_MAGIC	0x0162
	u_short	magic;		/* The magic number. */

	u_short	numSections;	/* The number of sections. */
	long	timeDateStamp;	/* Time and date stamp. */		
	long	symPtr;		/* File pointer to symbolic header. */	
	long	numSyms;	/* Size of symbolic header. */
	u_short	optHeader;	/* Size of optional header. */
	u_short	flags;		/* Flags. */

/* Description of the a.out section. */
	short	aout_magic;	/* Magic number. */

	short	verStamp;	/* Version stamp. */
	long	codeSize;	/* Code size in bytes. */
	long	heapSize;	/* Initialized data size in bytes. */
	long	bssSize;	/* Uninitialized data size in bytes. */
	long	entry;		/* Entry point. */
	long	codeStart;	/* Base of code used for this file. */
	long	heapStart;	/* Base of heap used for this file. */
	long	bssStart;	/* Base of bss used for this file. */
	long	gprMask;	/* General purpose register mask. */
	long	cprMask[4];	/* Co-processor register masks. */
	long	gpValue;	/* The gp value for this object. */
};

/* Section header. */
typedef struct {
	char	name[8];	/* Section name. */
	long	physAddr;	/* Section physical address. */
	long	virtAddr;	/* Section virtual address. */
	long	size;		/* Section size. */
	long	sectionPtr;	/* File pointer to section data. */
	long	relocPtr;	/* File pointer to relocation data. */
	long	lnnoPtr;	/* File pointer to gp tables. */
	u_short	numReloc;	/* Number of relocation entries. */
	u_short	numLnno;	/* Numberof gp tables. */
	long	flags;		/* Section flags. */
} ProcSectionHeader;

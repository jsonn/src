/*	$NetBSD: bootconfig.h,v 1.7.12.1 2000/11/20 20:03:59 bouyer Exp $	*/

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * boot configuration structures
 *
 * Created      : 12/09/94
 *
 * Based on kate/boot/bootconfig.h
 */

#include "opt_footbridge.h"

typedef struct _PhysMem {
	u_int address;
	u_int pages;
} PhysMem;

#if defined(_KERNEL) && (defined(RISCPC) || defined(RC7500))

#define DRAM_BLOCKS	4
typedef struct _BootConfig {
	u_int kernvirtualbase;
	u_int kernphysicalbase;
	u_int kernsize;
	u_int argvirtualbase;
	u_int argphysicalbase;
	u_int argsize;
	u_int scratchvirtualbase;
	u_int scratchphysicalbase;
	u_int scratchsize;

	u_int display_start;
	u_int display_size;
	u_int width;
	u_int height;
	u_int bitsperpixel;

	PhysMem dram[DRAM_BLOCKS];
	PhysMem vram[1];

	u_int dramblocks;
	u_int vramblocks;
	u_int pagesize;
	u_int drampages;
	u_int vrampages;

	char kernelname[80];

	u_int framerate;
	u_char machine_id[4];
	u_int magic;
	u_int display_phys;
} BootConfig;

#define BOOTCONFIG_MAGIC 0x42301068

extern BootConfig bootconfig;
#endif	/* _KERNEL && (RISCPC || RC7500) */

#if defined(_KERNEL) && defined(FOOTBRIDGE)

#define	DRAM_BLOCKS	1

typedef struct _BootConfig {
	PhysMem dram[DRAM_BLOCKS];
	u_int dramblocks;
} BootConfig;

extern BootConfig bootconfig;
#define MAX_BOOT_STRING 255
#endif	/* _KERNEL && FOOTBRIDGE */

#if defined(_KERNEL) && defined(OFW)
/*
 * Currently several bootconfig structure members are used
 * in the arm32 generic part. This needs to be fixed.
 * In the mean time just define the fields required
 * to get the files to compile.
 * To solve this either
 * 1. fake a bootconfig structure as required
 * 2. provide a generic structure for this information
 *    (need to see the shark code first)
 * 3. move the dependant routines to the machine specific
 *    areas (e.g. move dumpsys() to *_machdep.c
 *
 * 1 is probably the simplest stop gap measure
 * 2 is the solution I plan on using.
 *
 * code affected: pmap.c stubs.c
 */

#define DRAM_BLOCKS	33

typedef struct _BootConfig {
	PhysMem dram[DRAM_BLOCKS];
	u_int dramblocks;
} BootConfig;

extern BootConfig bootconfig;
#endif	/* _KERNEL && OFW */

#ifdef _KERNEL
#define BOOTOPT_TYPE_BOOLEAN		0
#define BOOTOPT_TYPE_STRING		1
#define BOOTOPT_TYPE_INT		2
#define BOOTOPT_TYPE_BININT		3
#define BOOTOPT_TYPE_HEXINT		4
#define BOOTOPT_TYPE_MASK		7

int get_bootconf_option __P((char *string, char *option, int type, void *result));

extern char *boot_args;
extern char *boot_file;
#endif	/* _KERNEL */

/* End of bootconfig.h */

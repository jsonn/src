/* $NetBSD: boot.c,v 1.4.2.2 1997/09/06 18:00:04 thorpej Exp $ */

/*
 * Copyright (c) 1992, 1993
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
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>

#include <machine/autoconf.h>
#include <machine/prom.h>

#include <machine/pte.h>

#include "common.h"

int loadfile __P((char *, u_int64_t *));

char boot_file[128];
char boot_flags[128];

struct bootinfo bootinfo;

extern char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

vm_offset_t ffp_save, ptbr_save;

extern vm_offset_t ssym, esym;

int debug;

char *kernelnames[] = {
	"netbsd",		"netbsd.gz",
	"netbsd.bak",		"netbsd.bak.gz",
	"netbsd.old",		"netbsd.old.gz",
	"onetbsd",		"onetbsd.gz",
	NULL
};

void
main()
{
	char *name, **namep;
	u_int64_t entry;
	int win;

	/* Init prom callback vector. */
	init_prom_calls();

	/* print a banner */
	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	printf("\n");

	/* switch to OSF pal code. */
	OSFpal();

	printf("\n");

	prom_getenv(PROM_E_BOOTED_FILE, boot_file, sizeof(boot_file));
	prom_getenv(PROM_E_BOOTED_OSFLAGS, boot_flags, sizeof(boot_flags));

	if (boot_file[0] != 0)
		(void)printf("Boot file: %s\n", boot_file);
	(void)printf("Boot flags: %s\n", boot_flags);

	if (boot_file[0] != '\0')
		win = (loadfile(name = boot_file, &entry) == 0);
	else
		for (namep = kernelnames, win = 0; *namep != NULL && !win;
		    namep++)
			win = (loadfile(name = *namep, &entry) == 0);

	printf("\n");
	if (win) {
		/*
		 * Fill in the bootinfo for the kernel.
		 */
		bzero(&bootinfo, sizeof(bootinfo));
		bootinfo.version    = 1;
		bootinfo.un.v1.ssym = ssym;
		bootinfo.un.v1.esym = esym;
		bcopy(name, bootinfo.un.v1.booted_kernel,
		    sizeof(bootinfo.un.v1.booted_kernel));
		bcopy(boot_flags, bootinfo.un.v1.boot_flags,
		    sizeof(bootinfo.un.v1.boot_flags));
		bootinfo.un.v1.hwrpb = NULL;
		bootinfo.un.v1.hwrpbsize = 0;
		bootinfo.un.v1.cngetc = NULL;
		bootinfo.un.v1.cnputc = NULL;
		bootinfo.un.v1.cnpollc = NULL;

		(void)printf("Entering %s at 0x%lx...\n", name, entry);
		(*(void (*)())entry)(ffp_save, ptbr_save,
		    BOOTINFO_MAGIC, &bootinfo, 0);
	}

	(void)printf("Boot failed!  Halting...\n");
	halt();
}

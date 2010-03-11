/* $NetBSD: machdep.c,v 1.4.16.2 2010/03/11 15:03:05 yamt Exp $ */

/*-
 * Copyright (c) 2007 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.4.16.2 2010/03/11 15:03:05 yamt Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/boot_flag.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_page.h>

#include "opt_memsize.h"

char machine[] = "usermode";
char machine_arch[] = "usermode";

int		usermode_x = IPL_NONE;
/* XXX */
int		physmem = MEMSIZE * 1024 / PAGE_SIZE;

void	main(int argc, char *argv[]);

void
main(int argc, char *argv[])
{
	extern void ttycons_consinit(void);
	extern void pmap_bootstrap(void);
	extern void kernmain(void);
	int i, j, r, tmpopt = 0;

	ttycons_consinit();

	for (i = 1; i < argc; i++) {
		for (j = 1; argv[i][j] != '\0'; j++) {
			r = 0;
			BOOT_FLAG(argv[i][j], r);
			if (r == 0) {
				printf("-%c: unknown flag\n", argv[i][j]);
				printf("usage: %s [-acdqsvxz]\n", argv[0]);
				printf("       (ex. \"%s -s\")\n", argv[0]);
				return;
			}
			tmpopt |= r;
		}
	}
	boothowto = tmpopt;

	uvm_setpagesize();
	uvmexp.ncolors = 2;

	pmap_bootstrap();

	kernmain();
}

void
setstatclockrate(int arg)
{
}

void
consinit(void)
{
	printf("NetBSD/usermode startup\n");
}

void
setregs(struct lwp *l, struct exec_package *pack, vaddr_t stack)
{
}

void
sendsig(const ksiginfo_t *ksi, const sigset_t *mask)
{
}

/*	$NetBSD: lkminit_syscall.c,v 1.5.10.1 2005/03/19 08:36:29 yamt Exp $	*/

/*
 * Makefile for newsyscall
 *
 * 05 Jun 93	Terry Lambert		Split example_syscall.c out
 * 25 May 93	Terry Lambert		Original
 *
 * Copyright (c) 1993 Terrence R. Lambert.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lkminit_syscall.c,v 1.5.10.1 2005/03/19 08:36:29 yamt Exp $");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <sys/file.h>
#include <sys/errno.h>


int	example_syscall __P((struct lwp *, void *, register_t *));
int	syscall_example_lkmentry __P((struct lkm_table *, int, int));

static int syscall_load __P((struct lkm_table *, int));

/*
 * These two entries define our system call and module information.  We
 * have 0 arguments to our system call.
 */
static struct sysent newent = {
	/* # of args, args size, MP-safe, fn. pointer */
	0, 0, 0, example_syscall
};

MOD_SYSCALL( "syscall_example", -1, &newent)

/*
 * This function is called each time the module is loaded.   Technically,
 * we could have made this "lkm_nofunc" in the "DISPATCH" in
 * "syscall_example_lkmentry()",
 * but it's a convenient place to kick a copyright out to the console.
 */
static int
syscall_load( lkmtp, cmd)
	struct lkm_table *lkmtp;
	int		  cmd;
{
	/* print copyright on console*/
	printf("Sample Loaded system call\n");
	printf("Copyright (c) 1993 Terrence R. Lambert\n");
	printf("All rights reserved\n");

	return (0);
}


/*
 * External entry point; should generally match name of .o file + '_lkmentry'.
 * The arguments are always the same for all loaded modules.  The "load",
 * "unload", and "stat" functions in "DISPATCH" will be called under
 * their respective circumstances.  If no function is desired, lkm_nofunc()
 * should be supplied.  They are called with the same arguments (cmd is
 * included to allow the use of a single function, ver is included for
 * version matching between modules and the kernel loader for the modules).
 *
 * Since we expect to link in the kernel and add external symbols to
 * the kernel symbol name space in a future version, generally all
 * functions used in the implementation of a particular module should
 * be static unless they are expected to be seen in other modules or
 * to resolve unresolved symbols alread existing in the kernel (the
 * second case is not likely to ever occur).
 *
 * The entry point should return 0 unless it is refusing load (in which
 * case it should return an errno from errno.h).
 */
int
syscall_example_lkmentry(lkmtp, cmd, ver)
	struct lkm_table *lkmtp;
	int	cmd, ver;
{
	DISPATCH(lkmtp,cmd,ver,syscall_load,lkm_nofunc,lkm_nofunc)
}

/*	$NetBSD: consinit.c,v 1.9.4.2 2002/10/10 18:36:32 jdolecek Exp $	*/

/*-
 * Copyright (c) 1999 Eduardo E. Horvath
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ddb.h"
#include "pcons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <machine/autoconf.h>
#include <machine/openfirm.h>
#include <machine/bsd_openprom.h>
#include <machine/cpu.h>
#include <machine/eeprom.h>
#include <machine/psl.h>
#include <machine/z8530var.h>
#include <machine/sparc64.h>

#include <dev/cons.h>

#include <sparc64/dev/cons.h>

static void prom_cnprobe __P((struct consdev *));
static void prom_cninit __P((struct consdev *));
int  prom_cngetc __P((dev_t));
static void prom_cnputc __P((dev_t, int));
static void prom_cnpollc __P((dev_t, int));
static void prom_cnputc __P((dev_t, int));

int stdin = NULL, stdout = NULL;

/*
 * The console is set to this one initially,
 * which lets us use the PROM until consinit()
 * is called to select a real console.
 */
struct consdev consdev_prom = {
	prom_cnprobe,
	prom_cninit,
	prom_cngetc,
	prom_cnputc,
	prom_cnpollc,
	NULL,
};

/*
 * The console table pointer is statically initialized
 * to point to the PROM (output only) table, so that
 * early calls to printf will work.
 */
struct consdev *cn_tab = &consdev_prom;

void
prom_cnprobe(cd)
	struct consdev *cd;
{
#if NPCONS > 0
	int maj;
	extern const struct cdevsw pcons_cdevsw;

	maj = cdevsw_lookup_major(&pcons_cdevsw);
	cd->cn_dev = makedev(maj, 0);
	cd->cn_pri = CN_INTERNAL;
#endif
}

int
prom_cngetc(dev)
	dev_t dev;
{
	unsigned char ch = '\0';
	int l;
#ifdef DDB
	static int nplus = 0;
#endif
	
	while ((l = OF_read(stdin, &ch, 1)) != 1)
		/* void */;
#ifdef DDB
	if (ch == '+') {
		if (nplus++ > 3) Debugger();
	} else nplus = 0;
#endif
	if (ch == '\r')
		ch = '\n';
	return ch;
}

static void
prom_cninit(cn)
	struct consdev *cn;
{
	if (!stdin) stdin = OF_stdin();
	if (!stdout) stdout = OF_stdout();
}

/*
 * PROM console output putchar.
 */
static void
prom_cnputc(dev, c)
	dev_t dev;
	int c;
{
	int s;
	char c0 = (c & 0x7f);

#if 0
	if (!stdout) stdout = OF_stdout();
#endif
	s = splhigh();
	OF_write(stdout, &c0, 1);
	splx(s);
}

void
prom_cnpollc(dev, on)
	dev_t dev;
	int on;
{
	if (on) {
                /* Entering debugger. */
#if NFB > 0
                fb_unblank();
#endif  
	} else {
                /* Resuming kernel. */
	}
#if NPCONS > 0
	pcons_cnpollc(dev, on);
#endif  
}

/*****************************************************************/

#ifdef	DEBUG
#define	DBPRINT(x)	prom_printf x
#else
#define	DBPRINT(x)
#endif

/*
 * This function replaces sys/dev/cninit.c
 * Determine which device is the console using
 * the PROM "input source" and "output sink".
 */
void
consinit()
{
	register int chosen;
	char buffer[128];
	extern int stdinnode, fbnode;
	char *consname = "unknown";
	
	DBPRINT(("consinit()\r\n"));
	if (cn_tab != &consdev_prom) return;
	
	DBPRINT(("setting up stdin\r\n"));
	chosen = OF_finddevice("/chosen");
	DBPRINT(("chosen = %x, stdin @ %p\r\n", chosen, &stdin));
	OF_getprop(chosen, "stdin",  &stdin, sizeof(stdin));
	DBPRINT(("stdin instance = %x\r\n", stdin));
	
	if ((stdinnode = OF_instance_to_package(stdin)) == 0) {
		printf("WARNING: no PROM stdin\n");
	} 
		
	DBPRINT(("stdin node = %x\r\n", stdinnode));
	DBPRINT(("setting up stdout\r\n"));
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
	
	DBPRINT(("stdout instance = %x\r\n", stdout));
	
	if ((fbnode = OF_instance_to_package(stdout)) == 0)
		printf("WARNING: no PROM stdout\n");
	
	DBPRINT(("stdout package = %x\r\n", fbnode));
	DBPRINT(("buffer @ %p\r\n", buffer));
	
	if (stdinnode && (OF_getproplen(stdinnode,"keyboard") >= 0)) {
#if NKBD > 0		
		printf("cninit: kdb/display not configured\n");
#endif
		consname = "keyboard/display";
	} else if (fbnode && 
		(OF_instance_to_path(stdin, buffer, sizeof(buffer)) >= 0)) {
		consname = buffer;
	}
	printf("console is %s\n", consname);
 
	/* Initialize PROM console */
	(*cn_tab->cn_probe)(cn_tab);
	(*cn_tab->cn_init)(cn_tab);
}


/*	$NetBSD: machdep.c,v 1.14.2.2 2002/03/16 15:58:49 jdolecek Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*-
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include "opt_ddb.h"
#include "opt_memsize.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/msgbuf.h>
#include <uvm/uvm_extern.h>
#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

#include <sh3/bscreg.h>
#include <sh3/cpgreg.h>
#include <sh3/cache_sh3.h>
#include <dev/cons.h>
#include <machine/bus.h>
#include <machine/mmeye.h>

/* the following is used externally (sysctl_hw) */
char machine[] = MACHINE;		/* mmeye */
char machine_arch[] = MACHINE_ARCH;	/* sh3eb */

paddr_t msgbuf_paddr;
extern paddr_t avail_start, avail_end;

#define IOM_RAM_END	((paddr_t)IOM_RAM_BEGIN + IOM_RAM_SIZE - 1)

void initSH3 __P((void *));
void LoadAndReset __P((char *));
void XLoadAndReset __P((char *));
void consinit __P((void));
void sh3_cache_on __P((void));
void InitializeBsc(void);

extern char start[], etext[], edata[], end[];

/*
 * Machine-dependent startup code
 *
 * This is called from main() in kern/main.c.
 */
void
cpu_startup()
{

	sh3_startup();
}

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	dev_t consdev;
	char *osimage;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
		    sizeof consdev));

	case CPU_LOADANDRESET:
		if (newp != NULL) {
			osimage = (char *)(*(u_long *)newp);

			LoadAndReset(osimage);
			/* not reach here */
		}
		return (0);

	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

int waittime = -1;

void
cpu_reboot(howto, bootstr)
	int howto;
	char *bootstr;
{
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		/* resettodr(); */
	}

	/* Disable interrupts. */
	splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();

haltsys:
	doshutdownhooks();

	if (howto & RB_HALT) {
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	}

	printf("rebooting...\n");
	cpu_reset();
	for(;;) ;
	/*NOTREACHED*/
}

void
initSH3(void *pc)	/* XXX return address */
{
	vaddr_t kernend;
	vsize_t sz;

	kernend = sh3_round_page(end);
#ifdef DDB
	/* XXX Currently symbol table size is not passed to the kernel. */
	kernend += 0x40000;					/* XXX */
#endif
	/* Clear bss */
	memset(edata, 0, end - edata);

	/* Initilize CPU ops. */
#if defined(SH7708R)
	sh_cpu_init(CPU_ARCH_SH3, CPU_PRODUCT_7708R);
#elif defined(SH7708)
	sh_cpu_init(CPU_ARCH_SH3, CPU_PRODUCT_7708);
#else
#warning "unknown product"
#endif
	/* Initialize proc0 and enable MMU. */
	sz = sh_proc0_init(kernend, IOM_RAM_BEGIN, IOM_RAM_END);

	/* Number of pages of physmem addr space */
	physmem = atop(IOM_RAM_END - IOM_RAM_BEGIN + 1);

	/* avail_start is first available physical memory address */
	avail_start = kernend + sz;
	avail_end = IOM_RAM_END + 1;

	consinit();
#ifdef DDB
	ddb_init(1, end, end + 0x40000);			/* XXX */
#endif

	/* Call pmap initialization to make new kernel address space */
	pmap_bootstrap(VM_MIN_KERNEL_ADDRESS);

	/*
	 * Initialize error message buffer (at end of core).
	 */
	initmsgbuf((caddr_t)msgbuf_paddr, round_page(MSGBUFSIZE));

	/*
	 * XXX We can't return here, because we change stack pointer.
	 *     So jump to return address directly.
	 */
	__asm __volatile (
		"jmp	@%0;"
		"mov	%1, r15" :: "r"(pc), "r"(proc0.p_addr->u_pcb.kr15));
}

/*
 * consinit:
 * initialize the system console.
 */
void
consinit()
{
	static int initted;

	if (initted)
		return;
	initted = 1;

	cninit();
}

int
bus_space_map(t, addr, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t addr;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	*bshp = (bus_space_handle_t)addr;

	return 0;
}

int
sh_memio_subregion(t, bsh, offset, size, nbshp)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{

	*nbshp = bsh + offset;
	return (0);
}

int
sh_memio_alloc(t, rstart, rend, size, alignment, boundary, flags,
	       bpap, bshp)
	bus_space_tag_t t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int flags;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	*bshp = *bpap = rstart;

	return (0);
}

void
sh_memio_free(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

}

void
sh_memio_unmap(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

}

/*
 * InitializeBsc
 * : BSC(Bus State Controler)
 */
void
InitializeBsc()
{
#ifdef NOPCMCIA
	/*
	 * Drive RAS,CAS in stand by mode and bus release mode
	 * Area0 = Normal memory, Area5,6=Normal(no burst)
	 * Area2 = Normal memory, Area3 = DRAM, Area5 = Normal memory
	 * Area4 = Normal Memory
	 * Area6 = Normal memory
	 */
	_reg_write_2(SH3_BCR1, 0x1010);
#else /* NOPCMCIA */
	/*
	 * Drive RAS,CAS in stand by mode and bus release mode
	 * Area0 = Normal memory, Area5,6=Normal(no burst)
	 * Area2 = Normal memory, Area3 = DRAM, Area5 = PCMCIA
	 * Area4 = Normal Memory
	 * Area6 = PCMCIA
	 */
	_reg_write_2(SH3_BCR1, 0x1013);
#endif /* NOPCMCIA */

#define PCMCIA_16
#ifdef PCMCIA_16
	/*
	 * Bus Width
	 * Area4: Bus width = 16bit
	 * Area6,5 = 16bit
	 * Area1 = 8bit
	 * Area2,3: Bus width = 32bit
	 */
	_reg_write_2(SH3_BCR2, 0x2af4);
#else /* PCMCIA16 */
	/*
	 * Bus Width
	 * Area4: Bus width = 16bit
	 * Area6,5 = 8bit
	 * Area1 = 8bit
	 * Area2,3: Bus width = 32bit
	 */
	_reg_write_2(SH3_BCR2, 0x16f4);
#endif /* PCMCIA16 */
	/*
	 * Idle cycle number in transition area and read to write
	 * Area6 = 3, Area5 = 3, Area4 = 3, Area3 = 3, Area2 = 3
	 * Area1 = 3, Area0 = 3
	 */
	_reg_write_2(SH3_WCR1, 0x3fff);

#if 0
	/*
	 * Wait cycle
	 * Area 6,5 = 2
	 * Area 4 = 10
	 * Area 3 = 2
	 * Area 2,1 = 3
	 * Area 0 = 6
	 */
	_reg_write_2(SH3_WCR2, 0x4bdd);
#else
	/*
	 * Wait cycle
	 * Area 6 = 6
	 * Area 5 = 2
	 * Area 4 = 10
	 * Area 3 = 3
	 * Area 2,1 = 3
	 * Area 0 = 6
	 */
	_reg_write_2(SH3_WCR2, 0xabfd);
#endif

	/*
	 * RAS pre-charge = 2cycle, RAS-CAS delay = 3 cycle,
	 * write pre-charge = 1cycle
	 * CAS before RAS refresh RAS assert time = 3  cycle
	 * Disable burst, Bus size=32bit, Column Address=10bit,Refresh ON
	 * CAS before RAS refresh ON, EDO DRAM
	 */
	_reg_write_2(SH3_MCR, 0x6135);
	/* SHREG_MCR = 0x4135; */

	/* DRAM Control Register */
	_reg_write_2(SH3_DCR, 0x0000);

	/*
	 * PCMCIA Control Register
	 * OE/WE assert delay 3.5 cycle
	 * OE/WE negate-address delay 3.5 cycle
	 */
	_reg_write_2(SH3_PCR, 0x00ff);

	/*
	 * Refresh Timer Control/Status Register
	 * Disable interrupt by CMF, closk 1/16, Disable OVF interrupt
	 * Count Limit = 1024
	 * In following statement, the reason why high byte = 0xa5(a4 in RFCR)
	 * is the rule of SH3 in writing these register .
	 */
	_reg_write_2(SH3_RTCSR, 0xa594);

	/*
	 * Refresh Timer Counter
	 * initialize to 0
	 */
	_reg_write_2(SH3_RTCNT, 0xa500);

	/*
	 * set Refresh Time Constant Register
	 */
	_reg_write_2(SH3_RTCOR, 0xa50d);

	/*
	 * init Refresh Count Register
	 */
	_reg_write_2(SH3_RFCR, 0xa400);

	/*
	 * Set Clock mode (make internal clock double speed)
	 */
#ifdef SH7708R
	_reg_write_2(SH3_FRQCR, 0xa100); /* 100MHz */
#else
	_reg_write_2(SH3_FRQCR, 0x0112); /* 60MHz */
#endif

#ifndef MMEYE_NO_CACHE
	/* Cache ON */
	_reg_write_4(SH3_CCR, SH3_CCR_CE);
#endif
}

void
sh3_cache_on(void)
{
#ifndef MMEYE_NO_CACHE
	/* Cache ON */
	_reg_write_4(SH3_CCR, SH3_CCR_CE);
	_reg_write_4(SH3_CCR, SH3_CCR_CF | SH3_CCR_CE);	/* cache clear */
	_reg_write_4(SH3_CCR, SH3_CCR_CE);
#endif
}

 /* XXX This value depends on physical available memory */
#define OSIMAGE_BUF_ADDR	(IOM_RAM_BEGIN + 0x00400000)

void
LoadAndReset(osimage)
	char *osimage;
{
	void *buf_addr;
	u_long size;
	u_long *src;
	u_long *dest;
	u_long csum = 0;
	u_long csum2 = 0;
	u_long size2;

	MMTA_IMASK = 0; /* mask all externel interrupt */

	printf("LoadAndReset: copy start\n");
	buf_addr = (void *)OSIMAGE_BUF_ADDR;

	size = *(u_long *)osimage;
	src = (u_long *)osimage;
	dest = buf_addr;

	size = (size + sizeof(u_long) * 2 + 3) >> 2;
	size2 = size;

	while (size--) {
		csum += *src;
		*dest++ = *src++;
	}

	dest = buf_addr;
	while (size2--)
		csum2 += *dest++;

	printf("LoadAndReset: copy end[%lx,%lx]\n", csum, csum2);
	printf("start XLoadAndReset\n");

	XLoadAndReset(buf_addr);
}

#ifdef sh3_tmp

#define	UART_BASE	0xa4000008

#define	IER	1
#define	IER_RBF	0x01
#define	IER_TBE	0x02
#define	IER_MSI	0x08
#define	FCR	2
#define	LCR	3
#define	LCR_DLAB	0x80
#define	DLM	1
#define	DLL	0
#define	MCR	4
#define	MCR_RTS	0x02
#define	MCR_DTR	0x01
#define	MCR_OUT2	0x08
#define	RTS_MODE	(MCR_RTS|MCR_DTR)
#define	LSR	5
#define	LSR_THRE	0x20
#define	LSR_ERROR	0x1e
#define	THR	0
#define	IIR	2
#define	IIR_II	0x06
#define	IIR_LSI	0x06
#define	IIR_MSI	0x00
#define	IIR_TBE	0x02
#define	IIR_PEND	0x01
#define	RBR	0
#define	MSR	6

#define	OUTP(port, val)	*(volatile unsigned char *)(UART_BASE+port) = val
#define	INP(port)	(*(volatile unsigned char *)(UART_BASE+port))

void
Init16550()
{
	int diviser;
	int tmp;

	/* Set speed */
	/* diviser = 12; */	/* 9600 bps */
	diviser = 6;	/* 19200 bps */

	OUTP(IER, 0);
	/* OUTP(FCR, 0x87); */	/* FIFO mode */
	OUTP(FCR, 0x00);	/* no FIFO mode */

	tmp = INP(LSR);
	tmp = INP(MSR);
	tmp = INP(IIR);
	tmp = INP(RBR);

	OUTP(LCR, INP(LCR) | LCR_DLAB);
	OUTP(DLM, 0xff & (diviser>>8));
	OUTP(DLL, 0xff & diviser);
	OUTP(LCR, INP(LCR) & ~LCR_DLAB);
	OUTP(MCR, 0);

	OUTP(LCR, 0x03);	/* 8 bit , no parity, 1 stop */

	/* start comm */
	OUTP(MCR, RTS_MODE | MCR_OUT2);
	/* OUTP(IER, IER_RBF | IER_TBE | IER_MSI); */
}

void
Send16550(int c)
{
	while (1) {
		OUTP(THR, c);

		while ((INP(LSR) & LSR_THRE) == 0)
			;

		if (c == '\n')
			c = '\r';
		else
			return;
	}
}
#endif /* sh3_tmp */

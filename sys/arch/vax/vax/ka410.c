/*	$NetBSD: ka410.c,v 1.3.6.1 1997/03/12 21:19:57 is Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/pmap.h>
#include <machine/nexus.h>
#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/clock.h>

static	void	ka410_conf __P((struct device*, struct device*, void*));
static	void	ka410_memenable __P((struct sbi_attach_args*, struct device *));
static	void	ka410_steal_pages __P((void));
static	void	ka410_memerr __P((void));
static	int	ka410_mchk __P((caddr_t));

static struct	ka410_clock *ka410_clkptr;

static	struct uc_map ka410_map[] = {
	{ KA410_CFGTST,		KA410_CFGTST+1023,	1024,	0 },
	{ KA410_ROM_BASE,	KA410_ROM_END,	KA410_ROM_SIZE, 0 },
	{ (int)KA410_CPU_BASE,	KA410_CPU_END,	KA410_CPU_SIZE, 0 },
	{ KA410_NWA_BASE,	KA410_NWA_END,	KA410_NWA_SIZE, 0 },
	{ KA410_SER_BASE,	KA410_SER_END,	KA410_SER_SIZE, 0 },
	{ (int)KA410_WAT_BASE,	KA410_WAT_END,	KA410_WAT_SIZE, 0 },
#if 0
	{ KA410_SCS_BASE,	KA410_SCS_END,	KA410_SCS_SIZE, 0 },
#else
	{ 0x200C0000,		0x200C01FF,	0x200,		0 },
#endif
	{ KA410_LAN_BASE,	KA410_LAN_END,	KA410_LAN_SIZE, 0 },
	{ KA410_CUR_BASE,	KA410_CUR_END,	KA410_CUR_SIZE, 0 },
	{ KA410_DMA_BASE,	KA410_DMA_END,	KA410_DMA_SIZE, 0 },
	/*
	 * there's more to come, eg. framebuffers (mono + GPX)
	 */
	{0, 0, 0, 0},
};

/* 
 * Declaration of 410-specific calls.
 */
struct	cpu_dep ka410_calls = {
	ka410_steal_pages,
	no_nicr_clock,
	ka410_mchk,
	ka410_memerr, 
	ka410_conf,
	ka410_clkread,
	ka410_clkwrite,
	1,      /* ~VUPS */
	(void*)KA410_INTREQ,      /* Used by vaxstation */
	(void*)KA410_INTCLR,      /* Used by vaxstation */
	(void*)KA410_INTMSK,      /* Used by vaxstation */
	ka410_map,
};


void
ka410_conf(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	extern char cpu_model[];

	if (vax_confdata & 0x80)	/* MSB in CFGTST */
		strcpy(cpu_model,"MicroVAX 2000");
	else
		strcpy(cpu_model,"VAXstation 2000");

	printf(": %s\n", cpu_model);
}

void
ka410_memerr()
{
	printf("Memory err!\n");
}

int
ka410_mchk(addr)
	caddr_t addr;
{
	panic("Machine check");
}

u_long le_iomem;			/* base addr of RAM -- CPU's view */
u_long le_ioaddr;			/* base addr of RAM -- LANCE's view */

void
ka410_steal_pages()
{
	extern	vm_offset_t avail_start, virtual_avail, avail_end;
	int	junk;

	int	i;
	struct {
		u_long     :2;
		u_long data:8;
		u_long     :22;
	} *p;
	int *srp;	/* Scratch Ram */
	char *q = (void*)&srp;

	srp = NULL;
	p = (void*)KA410_SCR;
	for (i=0; i<4; i++) {
	  printf("p[%d] = %x, ", i, p[i].data);
	  q[i]	= p[i].data;
	}
	p = (void*)KA410_SCRLEN;
	printf("\nlen = %d\n", p->data);
	printf("srp = 0x%x\n", srp);

	for (i=0; i<0x2; i++) {
		printf("%x:0x%x ", i*4, srp[i]);
		if ((i & 0x07) == 0x07)
			printf("\n");
	}
	printf("\n");

	/* 
	 * SCB is already copied/initialized at addr avail_start
	 * by pmap_bootstrap(), but it's not yet mapped. Thus we use
	 * the MAPPHYS() macro to reserve these two pages and to
	 * perform the mapping. The mapped address is assigned to junk.
	 */
	MAPPHYS(junk, 2, VM_PROT_READ|VM_PROT_WRITE);

	/*
	 * At top of physical memory there are some console-prom and/or
	 * restart-specific data. Make this area unavailable.
	 */
	avail_end -= 10 * NBPG;

	/*
	 * If we need to map physical areas also, we can decrease avail_end
	 * (the highest available memory-address), copy the stuff into the
	 * gap between and use pmap_map to map it...
	 *
	 * Don't use the MAPPHYS macro here, since this uses and changes(!)
	 * the value of avail_start. Use MAPVIRT even if it's name misleads.
	 */
	avail_end -= 10 * NBPG;		/* paranoid: has been done before */

	avail_end = (int)srp;

	avail_end &= ~0xffff;		/* make avail_end 64K-aligned */
	avail_end -= (64 * 1024);	/* steal 64K for LANCE's iobuf */
	le_ioaddr = avail_end;		/* ioaddr=phys, iomem=virt */
	MAPVIRT(le_iomem, (64 * 1024)/NBPG);
	pmap_map((vm_offset_t)le_iomem, le_ioaddr, le_ioaddr + 0xffff,
		 VM_PROT_READ|VM_PROT_WRITE);

	printf("le_iomem: %x, le_ioaddr: %x, srp: %x, avail_end: %x\n",
	       le_iomem, le_ioaddr, srp, avail_end);

	/*
	 * VAXstation 2000 and MicroVAX 2000: 
	 * since there's no bus, we have to map in anything which 
	 * could be neccessary/used/interesting...
	 * 
	 * MAPVIRT(ptr,count) reserves a virtual area with the requested size
	 *			and initializes ptr to point at this location
	 * pmap_map(ptr,...)  inserts a pair of virtual/physical addresses
	 *			into the system maptable (Sysmap)
	 */
	uvax_fillmap();

	/*
	 * Clear restart and boot in progress flags
	 * in the CPMBX. (ie. clear bits 4 and 5)
	 */
	KA410_WAT_BASE->cpmbx = (KA410_WAT_BASE->cpmbx & ~0x30);

	/*
	 * Enable memory parity error detection and clear error bits.
	 */
	KA410_CPU_BASE->ka410_mser = 1; 
	/* (UVAXIIMSER_PEN | UVAXIIMSER_MERR | UVAXIIMSER_LEB); */

	ka410_clkptr = (void*)uvax_phys2virt((u_long)KA410_WAT_BASE);
}
/*
 * define what we need and overwrite the uVAX_??? names
 */

#define uVAX_clock	ka410_clock
#define uVAX_clkptr	ka410_clkptr
#define uVAX_clkread	ka410_clkread
#define uVAX_clkwrite	ka410_clkwrite

#include <arch/vax/vax/uvax_proto.c>

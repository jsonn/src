/*	$NetBSD: ka860.c,v 1.6.6.1 1997/03/12 21:20:34 is Exp $	*/
/*
 * Copyright (c) 1986, 1988 Regents of the University of California.
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
 *	@(#)ka860.c	7.4 (Berkeley) 12/16/90
 */

/*
 * VAX 8600 specific routines.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/clock.h>
#include <machine/mtpr.h>
#include <machine/nexus.h>
#include <machine/ioa.h>
#include <machine/sid.h>

struct	ioa *ioa; 

void	ka86_conf __P((struct device *, struct device *, void *));
void	ka86_memenable __P((struct sbi_attach_args *, struct device *));
void	ka86_memerr __P((void));
int	ka86_mchk __P((caddr_t));
void	ka86_steal_pages __P((void));

void	crlattach __P((void));

struct	cpu_dep	ka860_calls = {
	ka86_steal_pages,
	generic_clock,
	ka86_mchk,
	ka86_memerr,
	ka86_conf,
	generic_clkread,
	generic_clkwrite,
	6,      /* ~VUPS */
	0,      /* Used by vaxstation */
	0,      /* Used by vaxstation */
	0,      /* Used by vaxstation */

};

/*
 * 8600 memory register (MERG) bit definitions
 */
#define M8600_ICRD	0x400		/* inhibit crd interrupts */
#define M8600_TB_ERR	0xf00		/* translation buffer error mask */

/*
 * MDECC register
 */
#define M8600_ADDR_PE	0x080000	/* address parity error */
#define M8600_DBL_ERR	0x100000	/* data double bit error */
#define M8600_SNG_ERR	0x200000	/* data single bit error */
#define M8600_BDT_ERR	0x400000	/* bad data error */

/*
 * ESPA register is used to address scratch pad registers in the Ebox.
 * To access a register in the scratch pad, write the ESPA with the address
 * and then read the ESPD register.  
 *
 * NOTE:  In assmebly code, the mfpr instruction that reads the ESPD
 *	  register must immedately follow the mtpr instruction that setup
 *	  the ESPA register -- per the VENUS processor register spec.
 *
 * The scratchpad registers that are supplied for a single bit ECC 
 * error are:
 */
#define SPAD_MSTAT1	0x25		/* scratch pad mstat1 register	*/
#define SPAD_MSTAT2	0x26		/* scratch pad mstat2 register	*/
#define SPAD_MDECC	0x27		/* scratch pad mdecc register	*/
#define SPAD_MEAR	0x2a		/* scratch pad mear register	*/

#define M8600_MEMERR(mdecc) ((mdecc) & 0x780000)
#define M8600_HRDERR(mdecc) ((mdecc) & 0x580000)
#define M8600_SYN(mdecc) (((mdecc) >> 9) & 0x3f)
#define M8600_ADDR(mear) ((mear) & 0x3ffffffc)
#define M8600_ARRAY(mear) (((mear) >> 22) & 0x0f)

#define M8600_MDECC_BITS \
"\20\27BAD_DT_ERR\26SNG_BIT_ERR\25DBL_BIT_ERR\24ADDR_PE"

#define M8600_MSTAT1_BITS "\20\30CPR_PE_A\27CPR_PE_B\26ABUS_DT_PE\
\25ABUS_CTL_MSK_PE\24ABUS_ADR_PE\23ABUS_C/A_CYCLE\22ABUS_ADP_1\21ABUS_ADP_0\
\20TB_MISS\17BLK_HIT\16C0_TAG_MISS\15CHE_MISS\14TB_VAL_ERR\13TB_PTE_B_PE\
\12TB_PTE_A_PE\11TB_TAG_PE\10WR_DT_PE_B3\7WR_DT_PE_B2\6WR_DT_PE_B1\
\5WR_DT_PE_B0\4CHE_RD_DT_PE\3CHE_SEL\2ANY_REFL\1CP_BW_CHE_DT_PE"

#define M8600_MSTAT2_BITS "\20\20CP_BYT_WR\17ABUS_BD_DT_CODE\10MULT_ERR\
\7CHE_TAG_PE\6CHE_TAG_W_PE\5CHE_WRTN_BIT\4NXM\3CP-IO_BUF_ERR\2MBOX_LOCK"

/* enable CRD reports */
void
ka86_memenable(sa, dev)
	struct sbi_attach_args *sa; /* XXX */
	struct device *dev;
{
	mtpr(mfpr(PR_MERG) & ~M8600_ICRD, PR_MERG);
}

/* log CRD errors */
void
ka86_memerr()
{
	register int reg11 = 0; /* known to be r11 below */
	int mdecc, mear, mstat1, mstat2, array;

	/*
	 * Scratchpad registers in the Ebox must be read by
	 * storing their ID number in ESPA and then immediately
	 * reading ESPD's contents with no other intervening
	 * machine instructions!
	 *
	 * The asm's below have a number of constants which
	 * are defined correctly above and in mtpr.h.
	 */
#ifdef lint
	reg11 = 0;
#else
	asm("mtpr $0x27,$0x4e; mfpr $0x4f,%0":: "r" (reg11));
#endif
	mdecc = reg11;	/* must acknowledge interrupt? */
	if (M8600_MEMERR(mdecc)) {
		asm("mtpr $0x2a,$0x4e; mfpr $0x4f,%0":: "r" (reg11));
		mear = reg11;
		asm("mtpr $0x25,$0x4e; mfpr $0x4f,%0":: "r" (reg11));
		mstat1 = reg11;
		asm("mtpr $0x26,$0x4e; mfpr $0x4f,%0":: "r" (reg11));
		mstat2 = reg11;
		array = M8600_ARRAY(mear);

		printf("mcr0: ecc error, addr %x (array %d) syn %x\n",
			M8600_ADDR(mear), array, M8600_SYN(mdecc));
		printf("\tMSTAT1 = %b\n\tMSTAT2 = %b\n",
			    mstat1, M8600_MSTAT1_BITS,
			    mstat2, M8600_MSTAT2_BITS);
		mtpr(0, PR_EHSR);
		mtpr(mfpr(PR_MERG) | M8600_ICRD, PR_MERG);
	}
}

#define NMC8600 7
char *mc8600[] = {
	"unkn type",	"fbox error",	"ebox error",	"ibox error",
	"mbox error",	"tbuf error",	"mbox 1D error"
};
/* codes for above */
#define MC_FBOX		1
#define MC_EBOX		2
#define MC_IBOX		3
#define MC_MBOX		4
#define MC_TBUF		5
#define MC_MBOX1D	6

/* error bits */
#define MBOX_FE		0x8000		/* Mbox fatal error */
#define FBOX_SERV	0x10000000	/* Fbox service error */
#define IBOX_ERR	0x2000		/* Ibox error */
#define EBOX_ERR	0x1e00		/* Ebox error */
#define MBOX_1D		0x81d0000	/* Mbox 1D error */
#define EDP_PE		0x200

struct mc8600frame {
	int	mc86_bcnt;		/* byte count == 0x58 */
	int	mc86_ehmsts;
	int	mc86_evmqsav;
	int	mc86_ebcs;
	int	mc86_edpsr;
	int	mc86_cslint;
	int	mc86_ibesr;
	int	mc86_ebxwd1;
	int	mc86_ebxwd2;
	int	mc86_ivasav;
	int	mc86_vibasav;
	int	mc86_esasav;
	int	mc86_isasav;
	int	mc86_cpc;
	int	mc86_mstat1;
	int	mc86_mstat2;
	int	mc86_mdecc;
	int	mc86_merg;
	int	mc86_cshctl;
	int	mc86_mear;
	int	mc86_medr;
	int	mc86_accs;
	int	mc86_cses;
	int	mc86_pc;		/* trapped pc */
	int	mc86_psl;		/* trapped psl */
};

/* machine check */
int
ka86_mchk(cmcf)
	caddr_t cmcf;
{
	register struct mc8600frame *mcf = (struct mc8600frame *)cmcf;
	register int type;

	if (mcf->mc86_ebcs & MBOX_FE)
		mcf->mc86_ehmsts |= MC_MBOX;
	else if (mcf->mc86_ehmsts & FBOX_SERV)
		mcf->mc86_ehmsts |= MC_FBOX;
	else if (mcf->mc86_ebcs & EBOX_ERR) {
		if (mcf->mc86_ebcs & EDP_PE)
			mcf->mc86_ehmsts |= MC_MBOX;
		else
			mcf->mc86_ehmsts |= MC_EBOX;
	} else if (mcf->mc86_ehmsts & IBOX_ERR)
		mcf->mc86_ehmsts |= MC_IBOX;
	else if (mcf->mc86_mstat1 & M8600_TB_ERR)
		mcf->mc86_ehmsts |= MC_TBUF;
	else if ((mcf->mc86_cslint & MBOX_1D) == MBOX_1D)
		mcf->mc86_ehmsts |= MC_MBOX1D;

	type = mcf->mc86_ehmsts & 0x7;
	printf("machine check %x: %s\n", type,
	    type < NMC8600 ? mc8600[type] : "???");
	printf("\tehm.sts %x evmqsav %x ebcs %x edpsr %x cslint %x\n",
	    mcf->mc86_ehmsts, mcf->mc86_evmqsav, mcf->mc86_ebcs,
	    mcf->mc86_edpsr, mcf->mc86_cslint);
	printf("\tibesr %x ebxwd %x %x ivasav %x vibasav %x\n",
	    mcf->mc86_ibesr, mcf->mc86_ebxwd1, mcf->mc86_ebxwd2,
	    mcf->mc86_ivasav, mcf->mc86_vibasav);
	printf("\tesasav %x isasav %x cpc %x mstat %x %x mdecc %x\n",
	    mcf->mc86_esasav, mcf->mc86_isasav, mcf->mc86_cpc,
	    mcf->mc86_mstat1, mcf->mc86_mstat2, mcf->mc86_mdecc);
	printf("\tmerg %x cshctl %x mear %x medr %x accs %x cses %x\n",
	    mcf->mc86_merg, mcf->mc86_cshctl, mcf->mc86_mear,
	    mcf->mc86_medr, mcf->mc86_accs, mcf->mc86_cses);
	printf("\tpc %x psl %x\n", mcf->mc86_pc, mcf->mc86_psl);
	mtpr(0, PR_EHSR);
	return (MCHK_PANIC);
}

void
ka86_steal_pages()
{
	extern	vm_offset_t avail_start, virtual_avail;
	extern	struct nexus *nexus;
	int	junk;
 
	/* 8600 may have 2 SBI's == 4 pages */
	MAPPHYS(junk, 4, VM_PROT_READ|VM_PROT_WRITE);

	/* Map in ioa register space */
	MAPVIRT(ioa, MAXNIOA);
	pmap_map((vm_offset_t)ioa, (u_int)IOA8600(0),
	    (u_int)IOA8600(0) + IOAMAPSIZ, VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)ioa + IOAMAPSIZ, (u_int)IOA8600(1),
	    (u_int)IOA8600(1) + IOAMAPSIZ, VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)ioa + 2 * IOAMAPSIZ, (u_int)IOA8600(2),
	    (u_int)IOA8600(2) + IOAMAPSIZ, VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)ioa + 3 * IOAMAPSIZ, (u_int)IOA8600(3),
	    (u_int)IOA8600(3) + IOAMAPSIZ, VM_PROT_READ|VM_PROT_WRITE);

	/* Map in possible nexus space */
	MAPVIRT(nexus, btoc(NEXSIZE * MAXNNEXUS));
	pmap_map((vm_offset_t)nexus, (u_int)NEXA8600,
	    (u_int)NEXA8600 + NNEX8600 * NEXSIZE, VM_PROT_READ|VM_PROT_WRITE);
	pmap_map((vm_offset_t)&nexus[NNEXSBI], (u_int)NEXB8600,
	    (u_int)NEXB8600 + NNEX8600 * NEXSIZE, VM_PROT_READ|VM_PROT_WRITE);
}

struct ka86 {
	unsigned snr:12,
		 plant:4,
		 eco:7,
		 v8650:1,
		 type:8;
};

void
ka86_conf(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	extern	char cpu_model[];
	struct	ka86 *ka86 = (void *)&vax_cpudata;

	/* Enable cache */
	mtpr(3, PR_CSWP);

	strcpy(cpu_model,"VAX 8600");
	if (ka86->v8650)
		cpu_model[5] = '5';
	printf(": %s, serial number %d(%d), hardware ECO level %d(%d)\n",
	    &cpu_model[4], ka86->snr, ka86->plant, ka86->eco >> 4, ka86->eco);
	printf("%s: ", self->dv_xname);
	if (mfpr(PR_ACCS) & 255) {
		printf("FPA present, type %d, serial number %d, enabling.\n", 
		    mfpr(PR_ACCS) & 255, mfpr(PR_ACCS) >> 16);
		mtpr(0x8000, PR_ACCS);
	} else
		printf("no FPA\n");
	crlattach();
}

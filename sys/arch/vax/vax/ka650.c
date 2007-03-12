/*	$NetBSD: ka650.c,v 1.31.8.1 2007/03/12 05:51:17 rmind Exp $	*/
/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mt. Xinu.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)ka650.c	7.7 (Berkeley) 12/16/90
 */

/*
 * vax650-specific code.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ka650.c,v 1.31.8.1 2007/03/12 05:51:17 rmind Exp $");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/ka650.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/mtpr.h>
#include <machine/sid.h>

struct	ka650_merr *ka650merr_ptr;
struct	ka650_cbd *ka650cbd_ptr;
struct	ka650_ssc *ka650ssc_ptr;
struct	ka650_ipcr *ka650ipcr_ptr;
int	*KA650_CACHE_ptr;

#define	CACHEOFF	0
#define	CACHEON		1

static	void	ka650setcache __P((int));
static	void	ka650_halt __P((void));
static	void	ka650_reboot __P((int));
static	void    uvaxIII_conf __P((void));
static	void    uvaxIII_memerr __P((void));
static	int     uvaxIII_mchk __P((void *));

struct	cpu_dep	ka650_calls = {
	0, /* No special page stealing anymore */
	uvaxIII_mchk,
	uvaxIII_memerr,
	uvaxIII_conf,
	generic_gettime,
	generic_settime,
	4,      /* ~VUPS */
	2,	/* SCB pages */
	ka650_halt,
	ka650_reboot,
	0,
	0,
	CPU_RAISEIPL,	/* Needed for the LANCE chip */
};

/*
 * uvaxIII_conf() is called by cpu_attach to do the cpu_specific setup.
 */
void
uvaxIII_conf()
{
	int syssub = GETSYSSUBT(vax_siedata);

	/*
	 * MicroVAX III: We map in memory error registers,
	 * cache control registers, SSC registers,
	 * interprocessor registers and cache diag space.
	 */
	ka650merr_ptr = (void *)vax_map_physmem(KA650_MERR, 1);
	ka650cbd_ptr = (void *)vax_map_physmem(KA650_CBD, 1);
	ka650ssc_ptr = (void *)vax_map_physmem(KA650_SSC, 3);
	ka650ipcr_ptr = (void *)vax_map_physmem(KA650_IPCR, 1);
	KA650_CACHE_ptr = (void *)vax_map_physmem(KA650_CACHE,
	    (KA650_CACHESIZE/VAX_NBPG));

	printf("cpu: KA6%d%d, CVAX microcode rev %d Firmware rev %d\n",
	    syssub == VAX_SIE_KA640 ? 4 : 5,
	    syssub == VAX_SIE_KA655 ? 5 : 0,
	    (vax_cpudata & 0xff), GETFRMREV(vax_siedata));
	ka650setcache(CACHEON);
	if (ctob(physmem) > ka650merr_ptr->merr_qbmbr) {
		printf("physmem(0x%x) > qbmbr(0x%x)\n",
		    ctob(physmem), (int)ka650merr_ptr->merr_qbmbr);
		panic("qbus map unprotected");
	}
	if (mfpr(PR_TODR) == 0)
		mtpr(1, PR_TODR);
}

void
uvaxIII_memerr()
{
	printf("memory err!\n");
#if 0 /* XXX Fix this */
	register char *cp = (char *)0;
	register int m;
	extern u_int cache2tag;

	if (ka650cbd.cbd_cacr & CACR_CPE) {
		printf("cache 2 tag parity error: ");
		if (time.tv_sec - cache2tag < 7) {
			ka650setcache(CACHEOFF);
			printf("cacheing disabled\n");
		} else {
			cache2tag = time.tv_sec;
			printf("flushing cache\n");
			ka650setcache(CACHEON);
		}
	}
	m = ka650merr.merr_errstat;
	ka650merr.merr_errstat = MEM_EMASK;
	if (m & MEM_CDAL) {
		cp = "Bus Parity";
	} else if (m & MEM_RDS) {
		cp = "Hard ECC";
	} else if (m & MEM_CRD) {
		cp = "Soft ECC";
	}
	if (cp) {
		printf("%sMemory %s Error: page 0x%x\n",
			(m & MEM_DMA) ? "DMA " : "", cp,
			(m & MEM_PAGE) >> MEM_PAGESHFT);
	}
#endif
}

#define NMC650	15
const char *mc650[] = {
	0,			"FPA proto err",	"FPA resv inst",
	"FPA Ill Stat 2",	"FPA Ill Stat 1",	"PTE in P0, TB miss",
	"PTE in P1, TB miss",	"PTE in P0, Mod",	"PTE in P1, Mod",
	"Illegal intr IPL",	"MOVC state error",	"bus read error",
	"SCB read error",	"bus write error",	"PCB write error"
};
u_int	cache1tag;
u_int	cache1data;
u_int	cdalerr;
u_int	cache2tag;

struct mc650frame {
	int	mc65_bcnt;		/* byte count == 0xc */
	int	mc65_summary;		/* summary parameter */
	int	mc65_mrvaddr;		/* most recent vad */
	int	mc65_istate1;		/* internal state */
	int	mc65_istate2;		/* internal state */
	int	mc65_pc;		/* trapped pc */
	int	mc65_psl;		/* trapped psl */
};

int
uvaxIII_mchk(cmcf)
	void *cmcf;
{
	register struct mc650frame *mcf = (struct mc650frame *)cmcf;
	register u_int type = mcf->mc65_summary;
	register u_int i;
	char sbuf[256];

	printf("machine check %x", type);
	if (type >= 0x80 && type <= 0x83)
		type -= (0x80 + 11);
	if (type < NMC650 && mc650[type])
		printf(": %s", mc650[type]);
	printf("\n\tvap %x istate1 %x istate2 %x pc %x psl %x\n",
	    mcf->mc65_mrvaddr, mcf->mc65_istate1, mcf->mc65_istate2,
	    mcf->mc65_pc, mcf->mc65_psl);
	bitmask_snprintf(ka650merr_ptr->merr_dser, DMASER_BITS,
			 sbuf, sizeof(sbuf));
	printf("dmaser=0x%s qbear=0x%x dmaear=0x%x\n", sbuf,
	    (int)ka650merr_ptr->merr_qbear,
	    (int)ka650merr_ptr->merr_dear);
	ka650merr_ptr->merr_dser = DSER_CLEAR;

	i = mfpr(PR_CAER);
	mtpr(CAER_MCC | CAER_DAT | CAER_TAG, PR_CAER);
	if (i & CAER_MCC) {
		printf("cache 1 ");
		if (i & CAER_DAT) {
			printf("data");
			i = cache1data;
			cache1data = time.tv_sec;
		}
		if (i & CAER_TAG) {
			printf("tag");
			i = cache1tag;
			cache1tag = time.tv_sec;
		}
	} else if ((i & CAER_MCD) || (ka650merr_ptr->merr_errstat & MEM_CDAL)) {
		printf("CDAL");
		i = cdalerr;
		cdalerr = time.tv_sec;
	}
	if (time.tv_sec - i < 7) {
		ka650setcache(CACHEOFF);
		printf(" parity error:	cacheing disabled\n");
	} else {
		printf(" parity error:	flushing cache\n");
		ka650setcache(CACHEON);
	}
	/*
	 * May be able to recover if type is 1-4, 0x80 or 0x81, but
	 * only if FPD is set in the saved PSL, or bit VCR in Istate2
	 * is clear.
	 */
	if ((type > 0 && type < 5) || type == 11 || type == 12) {
		if ((mcf->mc65_psl & PSL_FPD)
		    || !(mcf->mc65_istate2 & IS2_VCR)) {
			uvaxIII_memerr();
			return 0;
		}
	}
	return -1;
}

/*
 * Make sure both caches are off and not in diagnostic mode.  Clear the
 * 2nd level cache (by writing to each quadword entry), then enable it.
 * Enable 1st level cache too.
 */
void
ka650setcache(int state)
{
	int syssub = GETSYSSUBT(vax_siedata);
	int i;

	/*
	 * Before doing anything, disable the cache.
	 */
	mtpr(0, PR_CADR);
	if (syssub != VAX_SIE_KA640)
		ka650cbd_ptr->cbd_cacr = CACR_CPE;

	/*
	 * Check what we want to do, enable or disable.
	 */
	if (state == CACHEON) {
		mtpr(CADR_SEN2 | CADR_SEN1 | CADR_CENI | CADR_CEND, PR_CADR);
		if (syssub != VAX_SIE_KA640) {
			for (i = 0;
			    i < (KA650_CACHESIZE / sizeof(KA650_CACHE_ptr[0]));
			    i += 2)
				KA650_CACHE_ptr[i] = 0;
			ka650cbd_ptr->cbd_cacr = CACR_CEN;
		}
	}
}

static void
ka650_halt()
{
	ka650ssc_ptr->ssc_cpmbx = CPMB650_DOTHIS | CPMB650_HALT;
	__asm("halt");
}

static void
ka650_reboot(arg)
	int arg;
{
	ka650ssc_ptr->ssc_cpmbx = CPMB650_DOTHIS | CPMB650_REBOOT;
}

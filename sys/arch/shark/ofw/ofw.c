/*	$NetBSD: ofw.c,v 1.8.2.1 2002/03/17 23:43:57 thorpej Exp $	*/

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

/*
 *  Routines for interfacing between NetBSD and OFW.
 * 
 *  Parts of this could be moved to an MI file in time. -JJK
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/mbuf.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <machine/bus.h>
#include <machine/frame.h>
#include <machine/bootconfig.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw.h>

#include <netinet/in.h>

#if	BOOT_FW_DHCP
#include <nfs/bootdata.h>
#endif

#ifdef SHARK
#include "machine/pio.h"
#include "machine/isa_machdep.h"
#endif

#include "pc.h"
#include "isadma.h"

#define IO_VIRT_BASE (OFW_VIRT_BASE + OFW_VIRT_SIZE)
#define IO_VIRT_SIZE 0x01000000

/*
 *  Imported variables
 */
extern BootConfig bootconfig;	/* temporary, I hope */

#ifdef	DIAGNOSTIC
/* NOTE: These variables will be removed, well some of them */
extern u_int spl_mask;
extern u_int current_mask;
#endif

extern int ofw_handleticks;


/*
 *  Imported routines
 */
extern void dump_spl_masks  __P((void));
extern void dumpsys	    __P((void));
extern void dotickgrovelling __P((vm_offset_t));
#if defined(SHARK) && (NPC > 0)
extern void shark_screen_cleanup __P((int));
#endif

#define WriteWord(a, b) \
*((volatile unsigned int *)(a)) = (b)

#define ReadWord(a) \
(*((volatile unsigned int *)(a)))


/*
 *  Exported variables
 */
/* These should all be in a meminfo structure. */
vm_offset_t physical_start;
vm_offset_t physical_freestart;
vm_offset_t physical_freeend;
vm_offset_t physical_end;
u_int free_pages;
int physmem;
pv_addr_t systempage;
#ifndef	OFWGENCFG
pv_addr_t irqstack;
#endif
pv_addr_t undstack;
pv_addr_t abtstack;
pv_addr_t kernelstack;

vm_offset_t msgbufphys;

/* for storage allocation, used to be local to ofw_construct_proc0_addrspace */
static vm_offset_t  virt_freeptr;	    

int ofw_callbacks = 0;		/* debugging counter */

/**************************************************************/


/*
 *  Declarations and definitions private to this module
 *
 */

struct mem_region {
	vm_offset_t start;
	vm_size_t size;
};

struct mem_translation {
	vm_offset_t virt;
	vm_size_t size;
	vm_offset_t phys;
	unsigned int mode;
};

struct isa_range {
	vm_offset_t isa_phys_hi;
	vm_offset_t isa_phys_lo;
	vm_offset_t parent_phys_start;
	vm_size_t   isa_size;
};

struct vl_range {
	vm_offset_t vl_phys_hi;
	vm_offset_t vl_phys_lo;
	vm_offset_t parent_phys_start;
	vm_size_t   vl_size;
};

struct vl_isa_range {
	vm_offset_t isa_phys_hi;
	vm_offset_t isa_phys_lo;
	vm_offset_t parent_phys_hi;
	vm_offset_t parent_phys_lo;
	vm_size_t   isa_size;
};

struct dma_range {
	vm_offset_t start;
	vm_size_t   size;
};

struct ofw_cbargs {
	char *name;
	int nargs;
	int nreturns;
	int args_n_results[12];
};


/* Memory info */
static int nOFphysmem;
static struct mem_region *OFphysmem;
static int nOFphysavail;
static struct mem_region *OFphysavail;
static int nOFtranslations;
static struct mem_translation *OFtranslations;
static int nOFdmaranges;
static struct dma_range *OFdmaranges;

/* The OFW client services handle. */
/* Initialized by ofw_init(). */
static ofw_handle_t ofw_client_services_handle;


static void ofw_callbackhandler __P((struct ofw_cbargs *));
static void ofw_construct_proc0_addrspace __P((pv_addr_t *, pv_addr_t *));
static void ofw_getphysmeminfo __P((void));
static void ofw_getvirttranslations __P((void));
static void *ofw_malloc(vm_size_t size);
static void ofw_claimpages __P((vm_offset_t *, pv_addr_t *, vm_size_t));
static void ofw_discardmappings __P ((vm_offset_t, vm_offset_t, vm_size_t));
static int ofw_mem_ihandle  __P((void));
static int ofw_mmu_ihandle  __P((void));
static vm_offset_t ofw_claimphys __P((vm_offset_t, vm_size_t, vm_offset_t));
#if 0
static vm_offset_t ofw_releasephys __P((vm_offset_t, vm_size_t));
#endif
static vm_offset_t ofw_claimvirt __P((vm_offset_t, vm_size_t, vm_offset_t));
static void ofw_settranslation __P ((vm_offset_t, vm_offset_t, vm_size_t, int));
static void ofw_initallocator __P((void));
static void ofw_configisaonly __P((vm_offset_t *, vm_offset_t *));
static void ofw_configvl __P((int, vm_offset_t *, vm_offset_t *));
static vm_offset_t ofw_valloc __P((vm_offset_t, vm_offset_t));


/*
 * DHCP hooks.  For a first cut, we look to see if there is a DHCP
 * packet that was saved by the firmware.  If not, we proceed as before,
 * getting hand-configured data from NVRAM.  If there is one, we get the
 * packet, and extract the data from it.  For now, we hand that data up
 * in the boot_args string as before.
 */


/**************************************************************/


/*
 *
 *  Support routines for xxx_machdep.c
 *
 *  The intent is that all OFW-based configurations use the
 *  exported routines in this file to do their business.  If
 *  they need to override some function they are free to do so.
 *
 *  The exported routines are:
 *
 *    openfirmware
 *    ofw_init
 *    ofw_boot
 *    ofw_getbootinfo
 *    ofw_configmem
 *    ofw_configisa
 *    ofw_configisadma
 *    ofw_gettranslation
 *    ofw_map
 *    ofw_getcleaninfo
 */


int
openfirmware(args)
	void *args;
{
	int ofw_result;
	u_int saved_irq_state;

	/* OFW is not re-entrant, so we wrap a mutex around the call. */
	saved_irq_state = disable_interrupts(I32_bit);
	ofw_result = ofw_client_services_handle(args);
	(void)restore_interrupts(saved_irq_state);

	return(ofw_result);
}


void
ofw_init(ofw_handle)
	ofw_handle_t ofw_handle;
{
	ofw_client_services_handle = ofw_handle;

	/*  Everything we allocate in the remainder of this block is
	 *  constrained to be in the "kernel-static" portion of the
	 *  virtual address space (i.e., 0xF0000000 - 0xF1000000).
	 *  This is because all such objects are expected to be in
	 *  that range by NetBSD, or the objects will be re-mapped
	 *  after the page-table-switch to other specific locations.
	 *  In the latter case, it's simplest if our pre-switch handles
	 *  on those objects are in regions that are already "well-
	 *  known."  (Otherwise, the cloning of the OFW-managed address-
	 *  space becomes more awkward.)  To minimize the number of L2
	 *  page tables that we use, we are further restricting the
	 *  remaining allocations in this block to the bottom quarter of
	 *  the legal range.  OFW will have loaded the kernel text+data+bss
	 *  starting at the bottom of the range, and we will allocate
	 *  objects from the top, moving downwards.  The two sub-regions
	 *  will collide if their total sizes hit 4MB.  The current total
	 *  is <1.5MB, so we aren't in any real danger yet.  The variable
	 *  virt-freeptr represents the next free va (moving downwards).
	 */
	virt_freeptr = KERNEL_BASE + 0x00400000;
}


void
ofw_boot(howto, bootstr)
	int howto;
	char *bootstr;
{

#ifdef DIAGNOSTIC
	printf("boot: howto=%08x curproc=%p\n", howto, curproc);
	printf("current_mask=%08x spl_mask=%08x\n", current_mask, spl_mask);

	printf("ipl_bio=%08x ipl_net=%08x ipl_tty=%08x ipl_vm=%08x\n",
	    irqmasks[IPL_BIO], irqmasks[IPL_NET], irqmasks[IPL_TTY],
	    irqmasks[IPL_VM]);
	printf("ipl_audio=%08x ipl_clock=%08x ipl_none=%08x\n",
	    irqmasks[IPL_AUDIO], irqmasks[IPL_CLOCK], irqmasks[IPL_NONE]);

	dump_spl_masks();
#endif

	/*
	 * If we are still cold then hit the air brakes
	 * and crash to earth fast
	 */
	if (cold) {
		doshutdownhooks();
		printf("Halted while still in the ICE age.\n");
		printf("The operating system has halted.\n");
		goto ofw_exit;
		/*NOTREACHED*/
	}

	/*
	 * If RB_NOSYNC was not specified sync the discs.
	 * Note: Unless cold is set to 1 here, syslogd will die during the unmount.
	 * It looks like syslogd is getting woken up only to find that it cannot
	 * page part of the binary in as the filesystem has been unmounted.
	 */
	if (!(howto & RB_NOSYNC))
		bootsync();

	/* Say NO to interrupts */
	splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();
	
	/* Run any shutdown hooks */
	doshutdownhooks();

	/* Make sure IRQ's are disabled */
	IRQdisable;

	if (howto & RB_HALT) {
		printf("The operating system has halted.\n");
		goto ofw_exit;
	}

	/* Tell the user we are booting */
	printf("rebooting...\n");

	/* Jump into the OFW boot routine. */
	{
		static char str[256];
		char *ap = str, *ap1 = ap;

		if (bootstr && *bootstr) {
			if (strlen(bootstr) > sizeof str - 5)
				printf("boot string too large, ignored\n");
			else {
				strcpy(str, bootstr);
				ap1 = ap = str + strlen(str);
				*ap++ = ' ';
			}
		}
		*ap++ = '-';
		if (howto & RB_SINGLE)
			*ap++ = 's';
		if (howto & RB_KDB)
			*ap++ = 'd';
		*ap++ = 0;
		if (ap[-2] == '-')
			*ap1 = 0;
#if defined(SHARK) && (NPC > 0)
		shark_screen_cleanup(0);
#endif
		OF_boot(str);
		/*NOTREACHED*/
	}

ofw_exit:
	printf("Calling OF_exit...\n");
#if defined(SHARK) && (NPC > 0)
	shark_screen_cleanup(1);
#endif
	OF_exit();
	/*NOTREACHED*/
}


#if	BOOT_FW_DHCP

extern	char	*ip2dotted	__P((struct in_addr));

/*
 * Get DHCP data from OFW
 */

void
get_fw_dhcp_data(bdp)
	struct bootdata *bdp;
{
	int chosen;
	int dhcplen;

	bzero((char *)bdp, sizeof(*bdp));
	if ((chosen = OF_finddevice("/chosen")) == -1)
		panic("no /chosen from OFW");
	if ((dhcplen = OF_getproplen(chosen, "bootp-response")) > 0) {
		u_char *cp;
		int dhcp_type = 0;
		char *ip;

		/*
		 * OFW saved a DHCP (or BOOTP) packet for us.
		 */
		if (dhcplen > sizeof(bdp->dhcp_packet))
			panic("DHCP packet too large");
		OF_getprop(chosen, "bootp-response", &bdp->dhcp_packet,
		    sizeof(bdp->dhcp_packet));
		SANITY(bdp->dhcp_packet.op == BOOTREPLY, "bogus DHCP packet");
		/*
		 * Collect the interesting data from DHCP into
		 * the bootdata structure.
		 */
		bdp->ip_address = bdp->dhcp_packet.yiaddr;
		ip = ip2dotted(bdp->ip_address);
		if (bcmp(bdp->dhcp_packet.options, DHCP_OPTIONS_COOKIE, 4) == 0)
			parse_dhcp_options(&bdp->dhcp_packet,
			    bdp->dhcp_packet.options + 4,
			    &bdp->dhcp_packet.options[dhcplen
			    - DHCP_FIXED_NON_UDP], bdp, ip);
		if (bdp->root_ip.s_addr == 0)
			bdp->root_ip = bdp->dhcp_packet.siaddr;
		if (bdp->swap_ip.s_addr == 0)
			bdp->swap_ip = bdp->dhcp_packet.siaddr;
	}
	/*
	 * If the DHCP packet did not contain all the necessary data,
	 * look in NVRAM for the missing parts.
	 */
	{
		int options;
		int proplen;
#define BOOTJUNKV_SIZE	256
		char bootjunkv[BOOTJUNKV_SIZE];	/* minimize stack usage */


		if ((options = OF_finddevice("/options")) == -1)
			panic("can't find /options");
		if (bdp->ip_address.s_addr == 0 &&
		    (proplen = OF_getprop(options, "ipaddr",
		    bootjunkv, BOOTJUNKV_SIZE - 1)) > 0) {
			bootjunkv[proplen] = '\0';
			if (dotted2ip(bootjunkv, &bdp->ip_address.s_addr) == 0)
				bdp->ip_address.s_addr = 0;
		}
		if (bdp->ip_mask.s_addr == 0 &&
		    (proplen = OF_getprop(options, "netmask",
		    bootjunkv, BOOTJUNKV_SIZE - 1)) > 0) {
			bootjunkv[proplen] = '\0';
			if (dotted2ip(bootjunkv, &bdp->ip_mask.s_addr) == 0)
				bdp->ip_mask.s_addr = 0;
		}
		if (bdp->hostname[0] == '\0' &&
		    (proplen = OF_getprop(options, "hostname",
		    bdp->hostname, sizeof(bdp->hostname) - 1)) > 0) {
			bdp->hostname[proplen] = '\0';
		}
		if (bdp->root[0] == '\0' &&
		    (proplen = OF_getprop(options, "rootfs",
		    bootjunkv, BOOTJUNKV_SIZE - 1)) > 0) {
			bootjunkv[proplen] = '\0';
			parse_server_path(bootjunkv, &bdp->root_ip, bdp->root);
		}
		if (bdp->swap[0] == '\0' &&
		    (proplen = OF_getprop(options, "swapfs",
		    bootjunkv, BOOTJUNKV_SIZE - 1)) > 0) {
			bootjunkv[proplen] = '\0';
			parse_server_path(bootjunkv, &bdp->swap_ip, bdp->swap);
		}
	}
}

#endif	/* BOOT_FW_DHCP */

void
ofw_getbootinfo(bp_pp, ba_pp)
	char **bp_pp;
	char **ba_pp;
{
	int chosen;
	int bp_len;
	int ba_len;
	char *bootpathv;
	char *bootargsv;

	/* Read the bootpath and bootargs out of OFW. */
	/* XXX is bootpath still interesting?  --emg */
	if ((chosen = OF_finddevice("/chosen")) == -1)
		panic("no /chosen from OFW");
	bp_len = OF_getproplen(chosen, "bootpath");
	ba_len = OF_getproplen(chosen, "bootargs");
	if (bp_len < 0 || ba_len < 0)
		panic("can't get boot data from OFW");

	bootpathv = (char *)ofw_malloc(bp_len);
	bootargsv = (char *)ofw_malloc(ba_len);

	if (bp_len)
		OF_getprop(chosen, "bootpath", bootpathv, bp_len);
	else
		bootpathv[0] = '\0';

	if (ba_len)
		OF_getprop(chosen, "bootargs", bootargsv, ba_len);
	else
		bootargsv[0] = '\0';

	*bp_pp = bootpathv;
	*ba_pp = bootargsv;
#ifdef DIAGNOSTIC
	printf("bootpath=<%s>, bootargs=<%s>\n", bootpathv, bootargsv);
#endif
}

vm_offset_t
ofw_getcleaninfo(void)
{
	int cpu;
	vm_offset_t vclean, pclean;

	if ((cpu = OF_finddevice("/cpu")) == -1)
		panic("no /cpu from OFW");

	if ((OF_getprop(cpu, "d-cache-flush-address", &vclean, 
	    sizeof(vclean))) != sizeof(vclean)) {
#ifdef DEBUG
		printf("no OFW d-cache-flush-address property\n");
#endif
		return -1;
	}

	if ((pclean = ofw_gettranslation(
	    of_decode_int((unsigned char *)&vclean))) == -1)
	panic("OFW failed to translate cache flush address");

	return pclean;
}

void
ofw_configisa(pio, pmem)
	vm_offset_t *pio;
	vm_offset_t *pmem;
{
	int vl;

	if ((vl = OF_finddevice("/vlbus")) == -1) /* old style OFW dev info tree */
		ofw_configisaonly(pio, pmem);
	else /* old style OFW dev info tree */
		ofw_configvl(vl, pio, pmem);
}

static void
ofw_configisaonly(pio, pmem)
	vm_offset_t *pio;
	vm_offset_t *pmem;
{
	int isa;
	int rangeidx;
	int size;
	vm_offset_t hi, start;
	struct isa_range ranges[2];
  
	if ((isa = OF_finddevice("/isa")) == -1)
	panic("OFW has no /isa device node");

	/* expect to find two isa ranges: IO/data and memory/data */
	if ((size = OF_getprop(isa, "ranges", ranges, sizeof(ranges))) 
	    != sizeof(ranges))
		panic("unexpected size of OFW /isa ranges property: %d", size);

	*pio = *pmem = -1;

	for (rangeidx = 0; rangeidx < 2; ++rangeidx) {
		hi    = of_decode_int((unsigned char *)
		    &ranges[rangeidx].isa_phys_hi);
		start = of_decode_int((unsigned char *)
		    &ranges[rangeidx].parent_phys_start);

	if (hi & 1) { /* then I/O space */
		*pio = start;
	} else {
		*pmem = start;
	}
	} /* END for */

	if ((*pio == -1) || (*pmem == -1))
		panic("bad OFW /isa ranges property");

}

static void
ofw_configvl(vl, pio, pmem)
	int vl;
	vm_offset_t *pio;
	vm_offset_t *pmem;
{
	int isa;
	int ir, vr;
	int size;
	vm_offset_t hi, start;
	struct vl_isa_range isa_ranges[2];
	struct vl_range     vl_ranges[2];
  
	if ((isa = OF_finddevice("/vlbus/isa")) == -1)
		panic("OFW has no /vlbus/isa device node");

	/* expect to find two isa ranges: IO/data and memory/data */
	if ((size = OF_getprop(isa, "ranges", isa_ranges, sizeof(isa_ranges))) 
	    != sizeof(isa_ranges))
		panic("unexpected size of OFW /vlbus/isa ranges property: %d",
		     size);

	/* expect to find two vl ranges: IO/data and memory/data */
	if ((size = OF_getprop(vl, "ranges", vl_ranges, sizeof(vl_ranges))) 
	    != sizeof(vl_ranges))
		panic("unexpected size of OFW /vlbus ranges property: %d", size);

	*pio = -1;
	*pmem = -1;

	for (ir = 0; ir < 2; ++ir) {
		for (vr = 0; vr < 2; ++vr) {
			if ((isa_ranges[ir].parent_phys_hi
			    == vl_ranges[vr].vl_phys_hi) &&
			    (isa_ranges[ir].parent_phys_lo
			    == vl_ranges[vr].vl_phys_lo)) {
				hi    = of_decode_int((unsigned char *)
				    &isa_ranges[ir].isa_phys_hi);
				start = of_decode_int((unsigned char *)
				    &vl_ranges[vr].parent_phys_start);

				if (hi & 1) { /* then I/O space */
					*pio = start;
				} else {
					*pmem = start;
				}
			} /* END if */
		} /* END for */
	} /* END for */

	if ((*pio == -1) || (*pmem == -1))
		panic("bad OFW /isa ranges property");
}

void
ofw_configisadma(pdma)
	vm_offset_t *pdma;
{
	int root;
	int rangeidx;
	int size;
	struct dma_range *dr;
#if NISADMA > 0
	extern bus_dma_segment_t *pmap_isa_dma_ranges;
	extern int pmap_isa_dma_nranges;
#endif

	if ((root = OF_finddevice("/")) == -1 ||
	    (size = OF_getproplen(root, "dma-ranges")) <= 0 ||
	    (OFdmaranges = (struct dma_range *)ofw_malloc(size)) == 0 ||
 	    OF_getprop(root, "dma-ranges", OFdmaranges, size) != size)
		panic("bad / dma-ranges property");

	nOFdmaranges = size / sizeof(struct dma_range);

#if NISADMA > 0
	/* Allocate storage for non-OFW representation of the range. */
	pmap_isa_dma_ranges = ofw_malloc(nOFdmaranges *
	    sizeof(bus_dma_segment_t));
	if (pmap_isa_dma_ranges == NULL)
		panic("unable to allocate pmap_isa_dma_ranges");
	pmap_isa_dma_nranges = nOFdmaranges;
#endif

	for (rangeidx = 0, dr = OFdmaranges; rangeidx < nOFdmaranges; 
	    ++rangeidx, ++dr) {
		dr->start = of_decode_int((unsigned char *)&dr->start);
		dr->size = of_decode_int((unsigned char *)&dr->size);
#if NISADMA > 0
		pmap_isa_dma_ranges[rangeidx].ds_addr = dr->start;
		pmap_isa_dma_ranges[rangeidx].ds_len  = dr->size;
#endif
	}

#ifdef DEBUG
	printf("dma ranges size = %d\n", size);

	for (rangeidx = 0; rangeidx < nOFdmaranges; ++rangeidx) {
		printf("%08lx %08lx\n", 
		(u_long)OFdmaranges[rangeidx].start, 
		(u_long)OFdmaranges[rangeidx].size);
	}
#endif
}

/* 
 *  Memory configuration:
 *
 *  We start off running in the environment provided by OFW.
 *  This has the MMU turned on, the kernel code and data
 *  mapped-in at KERNEL_BASE (0xF0000000), OFW's text and
 *  data mapped-in at OFW_VIRT_BASE (0xF7000000), and (possibly)
 *  page0 mapped-in at 0x0.
 *
 *  The strategy is to set-up the address space for proc0 --
 *  including the allocation of space for new page tables -- while
 *  memory is still managed by OFW.  We then effectively create a
 *  copy of the address space by dumping all of OFW's translations
 *  and poking them into the new page tables.  We then notify OFW
 *  that we are assuming control of memory-management by installing
 *  our callback-handler, and switch to the NetBSD-managed page
 *  tables with the setttb() call.
 *  
 *  This scheme may cause some amount of memory to be wasted within
 *  OFW as dead page tables, but it shouldn't be more than about 
 *  20-30KB.  (It's also possible that OFW will re-use the space.)
 */
void
ofw_configmem(void)
{
	pv_addr_t proc0_ttbbase;
	pv_addr_t proc0_ptpt;

	/* Set-up proc0 address space. */
	ofw_construct_proc0_addrspace(&proc0_ttbbase, &proc0_ptpt);

	/*
	 * Get a dump of OFW's picture of physical memory.
	 * This is used below to initialize a load of variables used by pmap.
	 * We get it now rather than later because we are about to
	 * tell OFW to stop managing memory.
	 */
	ofw_getphysmeminfo();

	/* We are about to take control of memory-management from OFW.
	 * Establish callbacks for OFW to use for its future memory needs.
	 * This is required for us to keep using OFW services.
	 */

	/* First initialize our callback memory allocator. */
	ofw_initallocator();

	OF_set_callback((void(*)())ofw_callbackhandler);

	/* Switch to the proc0 pagetables. */
	setttb(proc0_ttbbase.pv_pa);

	/* Aaaaaaaah, running in the proc0 address space! */
	/* I feel good... */

	/* Set-up the various globals which describe physical memory for pmap. */
	{
		struct mem_region *mp;
		int totalcnt;
		int availcnt;
		int i;

		/* physmem, physical_start, physical_end */
		physmem = 0;
		for (totalcnt = 0, mp = OFphysmem; totalcnt < nOFphysmem; 
		    totalcnt++, mp++) {
#ifdef	OLDPRINTFS
			printf("physmem: %x, %x\n", mp->start, mp->size);
#endif
			physmem += btoc(mp->size);
		}
		physical_start = OFphysmem[0].start;
		mp--;
		physical_end = mp->start + mp->size;

		/* free_pages, physical_freestart, physical_freeend */
		free_pages = 0;
		for (availcnt = 0, mp = OFphysavail; availcnt < nOFphysavail;
		    availcnt++, mp++) {
#ifdef	OLDPRINTFS
			printf("physavail: %x, %x\n", mp->start, mp->size);
#endif
			free_pages += btoc(mp->size);
		}
		physical_freestart = OFphysavail[0].start;
		mp--;
		physical_freeend = mp->start + mp->size;
#ifdef	OLDPRINTFS
		printf("pmap_bootstrap:  physmem = %x, free_pages = %x\n",
		    physmem, free_pages);
#endif

		/*
		 *  This is a hack to work with the existing pmap code.
		 *  That code depends on a RiscPC BootConfig structure
		 *  containing, among other things, an array describing
		 *  the regions of physical memory.  So, for now, we need
		 *  to stuff our OFW-derived physical memory info into a
		 *  "fake" BootConfig structure.
		 *
		 *  An added twist is that we initialize the BootConfig
		 *  structure with our "available" physical memory regions
		 *  rather than the "total" physical memory regions.  Why?
		 *  Because:
		 *
		 *   (a) the VM code requires that the "free" pages it is
		 *       initialized with have consecutive indices.  This
		 *       allows it to use more efficient data structures
		 *       (presumably).
		 *   (b) the current pmap routines which report the initial
		 *       set of free page indices (pmap_next_page) and
		 *       which map addresses to indices (pmap_page_index)
		 *       assume that the free pages are consecutive across
		 *       memory region boundaries.
		 *
		 *  This means that memory which is "stolen" at startup time
		 *  (say, for page descriptors) MUST come from either the
		 *  bottom of the first region or the top of the last.
		 *
		 *  This requirement doesn't mesh well with OFW (or at least
		 *  our use of it).  We can get around it for the time being
		 *  by pretending that our "available" region array describes
		 *  all of our physical memory.  This may cause some important
		 *  information to be excluded from a dump file, but so far
		 *  I haven't come across any other negative effects.
		 *
		 *  In the long-run we should fix the index
		 *  generation/translation code in the pmap module.
		 */

		if (DRAM_BLOCKS < (availcnt + 1))
			panic("more ofw memory regions than bootconfig blocks");

		for (i = 0, mp = OFphysavail; i < nOFphysavail; i++, mp++) {
			bootconfig.dram[i].address = mp->start;
			bootconfig.dram[i].pages = btoc(mp->size);
		}
		bootconfig.dramblocks = availcnt;
	}

	/* Initialize pmap module. */
	pmap_bootstrap((pd_entry_t *)proc0_ttbbase.pv_va, proc0_ptpt);
}


/*
 ************************************************************

  Routines private to this module

 ************************************************************
 */

/* N.B.  Not supposed to call printf in callback-handler!  Could deadlock! */
static void
ofw_callbackhandler(args)
	struct ofw_cbargs *args;
{
	char *name = args->name;
	int nargs = args->nargs;
	int nreturns = args->nreturns;
	int *args_n_results = args->args_n_results;

	ofw_callbacks++;

#if defined(OFWGENCFG)
	/* Check this first, so that we don't waste IRQ time parsing. */
	if (strcmp(name, "tick") == 0) {
		vm_offset_t frame;

		/* Check format. */
		if (nargs != 1 || nreturns < 1) {
			args_n_results[nargs] = -1;
			args->nreturns = 1;
			return;
		}
		args_n_results[nargs] =	0;	/* properly formatted request */

		/*
		 *  Note that we are running in the IRQ frame, with interrupts
		 *  disabled.
		 *  
		 *  We need to do two things here:
		 *    - copy a few words out of the input frame into a global
		 *      area, for later use by our real tick-handling code
		 *    - patch a few words in the frame so that when OFW returns
		 *      from the interrupt it will resume with our handler
		 *      rather than the code that was actually interrupted.
		 *      Our handler will resume when it finishes with the code
		 *      that was actually interrupted.
		 *  
		 *  It's simplest to do this in assembler, since it requires
		 *  switching frames and grovelling about with registers.
		 */
		frame = (vm_offset_t)args_n_results[0];
		if (ofw_handleticks)
			dotickgrovelling(frame);
		args_n_results[nargs + 1] = frame;
		args->nreturns = 1;
	} else
#endif

	if (strcmp(name, "map") == 0) {
		vm_offset_t va;
		vm_offset_t pa;
		vm_size_t size;
		int mode;
		int ap_bits;
		int dom_bits;
		int cb_bits;

		/* Check format. */
		if (nargs != 4 || nreturns < 2) {
			args_n_results[nargs] = -1;
			args->nreturns = 1;
			return;
		}
		args_n_results[nargs] =	0;	/* properly formatted request */

		pa = (vm_offset_t)args_n_results[0];
		va = (vm_offset_t)args_n_results[1];
		size = (vm_size_t)args_n_results[2];
		mode = args_n_results[3];
		ap_bits =  (mode & 0x00000C00);
		dom_bits = (mode & 0x000001E0);
		cb_bits =  (mode & 0x000000C0);

		/* Sanity checks. */
		if ((va & PGOFSET) != 0 || va < OFW_VIRT_BASE ||
		    (va + size) > (OFW_VIRT_BASE + OFW_VIRT_SIZE) ||
		    (pa & PGOFSET) != 0 || (size & PGOFSET) != 0 ||
		    size == 0 || (dom_bits >> 5) != 0) {
			args_n_results[nargs + 1] = -1;
			args->nreturns = 1;
			return;
		}

		/* Write-back anything stuck in the cache. */
		cpu_idcache_wbinv_all();

		/* Install new mappings. */
		{
			pt_entry_t *pte = vtopte(va);
			int npages = size >> PGSHIFT;

			ap_bits >>= 10;
			for (; npages > 0; pte++, pa += NBPG, npages--)
				*pte = (pa | PT_AP(ap_bits) | L2_SPAGE | cb_bits);
		}

		/* Clean out tlb. */
		tlb_flush();

		args_n_results[nargs + 1] = 0;
		args->nreturns = 2;
	} else if (strcmp(name, "unmap") == 0) {
		vm_offset_t va;
		vm_size_t size;

		/* Check format. */
		if (nargs != 2 || nreturns < 1) {
			args_n_results[nargs] = -1;
			args->nreturns = 1;
			return;
		}
		args_n_results[nargs] =	0;	/* properly formatted request */

		va = (vm_offset_t)args_n_results[0];
		size = (vm_size_t)args_n_results[1];

		/* Sanity checks. */
		if ((va & PGOFSET) != 0 || va < OFW_VIRT_BASE ||
		    (va + size) > (OFW_VIRT_BASE + OFW_VIRT_SIZE) ||
		    (size & PGOFSET) != 0 || size == 0) {
			args_n_results[nargs + 1] = -1;
			args->nreturns = 1;
			return;
		}

		/* Write-back anything stuck in the cache. */
		cpu_idcache_wbinv_all();

		/* Zero the mappings. */
		{
			pt_entry_t *pte = vtopte(va);
			int npages = size >> PGSHIFT;

			for (; npages > 0; pte++, npages--)
				*pte = 0;
		}

		/* Clean out tlb. */
		tlb_flush();

		args->nreturns = 1;
	} else if (strcmp(name, "translate") == 0) {
		vm_offset_t va;
		vm_offset_t pa;
		int mode;
		pt_entry_t pte;

		/* Check format. */
		if (nargs != 1 || nreturns < 4) {
			args_n_results[nargs] = -1;
			args->nreturns = 1;
			return;
		}
		args_n_results[nargs] =	0;	/* properly formatted request */

		va = (vm_offset_t)args_n_results[0];

		/* Sanity checks.
		 * For now, I am only willing to translate va's in the
		 * "ofw range." Eventually, I may be more generous. -JJK
		 */
		if ((va & PGOFSET) != 0 ||  va < OFW_VIRT_BASE ||
		    va >= (OFW_VIRT_BASE + OFW_VIRT_SIZE)) {
			args_n_results[nargs + 1] = -1;
			args->nreturns = 1;
			return;
		}

		/* Lookup mapping. */
		pte = *vtopte(va);
		if (pte == 0) {
			/* No mapping. */
			args_n_results[nargs + 1] = -1;
			args->nreturns = 2;
		} else {
			/* Existing mapping. */
			pa = (pte & PG_FRAME) | (va & ~PG_FRAME);
			mode = (pte & 0x0C00) | (0 << 5) | (pte & 0x000C);	/* AP | DOM | CB */

			args_n_results[nargs + 1] = 0;
			args_n_results[nargs + 2] = pa;
			args_n_results[nargs + 3] =	mode;
			args->nreturns = 4;
		}
	} else if (strcmp(name, "claim-phys") == 0) {
		struct pglist alloclist = TAILQ_HEAD_INITIALIZER(alloclist);
		vm_offset_t low, high;
		vm_size_t align, size;

		/*
		 * XXX
		 * XXX THIS IS A GROSS HACK AND NEEDS TO BE REWRITTEN. -- cgd
		 * XXX
		 */

		/* Check format. */
		if (nargs != 4 || nreturns < 3) {
			args_n_results[nargs] = -1;
			args->nreturns = 1;
			return;
		}
		args_n_results[nargs] =	0;	/* properly formatted request */

		low = args_n_results[0];
		size = args_n_results[2];
		align = args_n_results[3];
		high = args_n_results[1] + size;

#if 0
		printf("claim-phys: low = 0x%x, size = 0x%x, align = 0x%x, high = 0x%x\n",
		    low, size, align, high);
		align = size;
		printf("forcing align to be 0x%x\n", align);
#endif

		args_n_results[nargs + 1] =
		uvm_pglistalloc(size, low, high, align, 0, &alloclist, 1, 0);
#if 0
		printf(" -> 0x%lx", args_n_results[nargs + 1]);
#endif
		if (args_n_results[nargs + 1] != 0) {
#if 0
			printf("(failed)\n");
#endif
			args_n_results[nargs + 1] = -1;
			args->nreturns = 2;
			return;
		} 
		args_n_results[nargs + 2] = alloclist.tqh_first->phys_addr;
#if 0
		printf("(succeeded: pa = 0x%lx)\n", args_n_results[nargs + 2]);
#endif
		args->nreturns = 3;

	} else if (strcmp(name, "release-phys") == 0) {
		printf("unimplemented ofw callback - %s\n", name);
		args_n_results[nargs] = -1;
		args->nreturns = 1;
	} else if (strcmp(name, "claim-virt") == 0) {
		vm_offset_t va;
		vm_size_t size;
		vm_offset_t align;

		/* XXX - notyet */
/*		printf("unimplemented ofw callback - %s\n", name);*/
		args_n_results[nargs] = -1;
		args->nreturns = 1;
		return;

		/* Check format. */
		if (nargs != 2 || nreturns < 3) {
		    args_n_results[nargs] = -1;
		    args->nreturns = 1;
		    return;
		}
		args_n_results[nargs] =	0;	/* properly formatted request */

		/* Allocate size bytes with specified alignment. */
		size = (vm_size_t)args_n_results[0];
		align = (vm_offset_t)args_n_results[1];
		if (align % NBPG != 0) {
			args_n_results[nargs + 1] = -1;
			args->nreturns = 2;
			return;
		}

		if (va == 0) {
			/* Couldn't allocate. */
			args_n_results[nargs + 1] = -1;
			args->nreturns = 2;
		} else {
			/* Successful allocation. */
			args_n_results[nargs + 1] = 0;
			args_n_results[nargs + 2] = va;
			args->nreturns = 3;
		}
	} else if (strcmp(name, "release-virt") == 0) {
		vm_offset_t va;
		vm_size_t size;

		/* XXX - notyet */
		printf("unimplemented ofw callback - %s\n", name);
		args_n_results[nargs] = -1;
		args->nreturns = 1;
		return;

		/* Check format. */
		if (nargs != 2 || nreturns < 1) {
			args_n_results[nargs] = -1;
			args->nreturns = 1;
			return;
		}
		args_n_results[nargs] =	0;	/* properly formatted request */

		/* Release bytes. */
		va = (vm_offset_t)args_n_results[0];
		size = (vm_size_t)args_n_results[1];

		args->nreturns = 1;
	} else {
		args_n_results[nargs] = -1;
		args->nreturns = 1;
	}
}

#define	KERNEL_VMDATA_PTS	(KERNEL_VM_SIZE >> (PDSHIFT + 2))
#define	KERNEL_OFW_PTS		4
#define	KERNEL_IO_PTS		4

static void
ofw_construct_proc0_addrspace(proc0_ttbbase, proc0_ptpt)
	pv_addr_t *proc0_ttbbase;
	pv_addr_t *proc0_ptpt;
{
	int i, oft;
	pv_addr_t proc0_pagedir;
	pv_addr_t proc0_pt_pte;
	pv_addr_t proc0_pt_sys;
	pv_addr_t proc0_pt_kernel;
	pv_addr_t proc0_pt_vmdata[KERNEL_VMDATA_PTS];
	pv_addr_t proc0_pt_ofw[KERNEL_OFW_PTS];
	pv_addr_t proc0_pt_io[KERNEL_IO_PTS];
	pv_addr_t msgbuf;
	vm_offset_t L1pagetable;
	struct mem_translation *tp;

	/* Set-up the system page. */
	systempage.pv_va = ofw_claimvirt(0, NBPG, 0);
	if (systempage.pv_va == -1) {
		/* Something was already mapped to VA 0. */
		systempage.pv_va = 0;
		systempage.pv_pa = ofw_gettranslation(0);
		if (systempage.pv_pa == -1)
			panic("bogus result from gettranslation(0)");
	} else {
		/* We were just allocated the page-length range at VA 0. */
		if (systempage.pv_va != 0)
			panic("bogus result from claimvirt(0, NBPG, 0)");

		/* Now allocate a physical page, and establish the mapping. */
		systempage.pv_pa = ofw_claimphys(0, NBPG, NBPG);
		if (systempage.pv_pa == -1)
			panic("bogus result from claimphys(0, NBPG, NBPG)");
		ofw_settranslation(systempage.pv_va, systempage.pv_pa,
		    NBPG, -1);	/* XXX - mode? -JJK */

		/* Zero the memory. */
		bzero((char *)systempage.pv_va, NBPG);
	}

	/* Allocate/initialize space for the proc0, NetBSD-managed */
	/* page tables that we will be switching to soon. */
	ofw_claimpages(&virt_freeptr, &proc0_pagedir, PD_SIZE);
	ofw_claimpages(&virt_freeptr, &proc0_pt_pte, PT_SIZE);
	ofw_claimpages(&virt_freeptr, &proc0_pt_sys, PT_SIZE);
	ofw_claimpages(&virt_freeptr, &proc0_pt_kernel, PT_SIZE);
	for (i = 0; i < KERNEL_VMDATA_PTS; i++)
		ofw_claimpages(&virt_freeptr, &proc0_pt_vmdata[i], PT_SIZE);
	for (i = 0; i < KERNEL_OFW_PTS; i++)
		ofw_claimpages(&virt_freeptr, &proc0_pt_ofw[i], PT_SIZE);
	for (i = 0; i < KERNEL_IO_PTS; i++)
		ofw_claimpages(&virt_freeptr, &proc0_pt_io[i], PT_SIZE);

	/* Allocate/initialize space for stacks. */
#ifndef	OFWGENCFG
	ofw_claimpages(&virt_freeptr, &irqstack, NBPG);
#endif
	ofw_claimpages(&virt_freeptr, &undstack, NBPG);
	ofw_claimpages(&virt_freeptr, &abtstack, NBPG);
	ofw_claimpages(&virt_freeptr, &kernelstack, UPAGES * NBPG);

	/* Allocate/initialize space for msgbuf area. */
	ofw_claimpages(&virt_freeptr, &msgbuf, MSGBUFSIZE);
	msgbufphys = msgbuf.pv_pa;

	/* Construct the proc0 L1 pagetable. */
	L1pagetable = proc0_pagedir.pv_va;

	pmap_link_l2pt(L1pagetable, 0x0, &proc0_pt_sys);
	pmap_link_l2pt(L1pagetable, KERNEL_BASE, &proc0_pt_kernel);
	pmap_link_l2pt(L1pagetable, PROCESS_PAGE_TBLS_BASE,
	    &proc0_pt_pte);
	for (i = 0; i < KERNEL_VMDATA_PTS; i++)
		pmap_link_l2pt(L1pagetable, KERNEL_VM_BASE + i * 0x00400000,
		    &proc0_pt_vmdata[i]);
	for (i = 0; i < KERNEL_OFW_PTS; i++)
		pmap_link_l2pt(L1pagetable, OFW_VIRT_BASE + i * 0x00400000,
		    &proc0_pt_ofw[i]);
	for (i = 0; i < KERNEL_IO_PTS; i++)
		pmap_link_l2pt(L1pagetable, IO_VIRT_BASE + i * 0x00400000,
		    &proc0_pt_io[i]);

	/*
	 * OK, we're done allocating.
	 * Get a dump of OFW's translations, and make the appropriate
	 * entries in the L2 pagetables that we just allocated.
	 */

	ofw_getvirttranslations();

	for (oft = 0,  tp = OFtranslations; oft < nOFtranslations;
	    oft++, tp++) {

		vm_offset_t va, pa;
		int npages = tp->size / NBPG;

		/* Size must be an integral number of pages. */
		if (npages == 0 || tp->size % NBPG != 0)
			panic("illegal ofw translation (size)");

		/* Make an entry for each page in the appropriate table. */
		for (va = tp->virt, pa = tp->phys; npages > 0;
		    va += NBPG, pa += NBPG, npages--) {
			/*
			 * Map the top bits to the appropriate L2 pagetable.
			 * The only allowable regions are page0, the
			 * kernel-static area, and the ofw area.
			 */
			switch (va >> (PDSHIFT + 2)) {
			case 0:
				/* page0 */
				break;

			case (KERNEL_BASE >> (PDSHIFT +	2)):
				/* kernel static area */
				break;

			case ( OFW_VIRT_BASE               >> (PDSHIFT + 2)):
			case ((OFW_VIRT_BASE + 0x00400000) >> (PDSHIFT + 2)):
			case ((OFW_VIRT_BASE + 0x00800000) >> (PDSHIFT + 2)):
			case ((OFW_VIRT_BASE + 0x00C00000) >> (PDSHIFT + 2)):
				/* ofw area */
				break;

			case ( IO_VIRT_BASE               >> (PDSHIFT + 2)):
			case ((IO_VIRT_BASE + 0x00400000) >> (PDSHIFT + 2)):
			case ((IO_VIRT_BASE + 0x00800000) >> (PDSHIFT + 2)):
			case ((IO_VIRT_BASE + 0x00C00000) >> (PDSHIFT + 2)):
				/* io area */
				break;

			default:
				/* illegal */
				panic("illegal ofw translation (addr) %#lx",
				    va);
			}

			/* Make the entry. */
			pmap_map_entry(L1pagetable, va, pa,
			    VM_PROT_READ|VM_PROT_WRITE,
			    (tp->mode & 0xC) == 0xC ? PTE_CACHE
						    : PTE_NOCACHE);
		}
	}

	/*
	 * We don't actually want some of the mappings that we just
	 * set up to appear in proc0's address space.  In particular,
	 * we don't want aliases to physical addresses that the kernel
	 * has-mapped/will-map elsewhere.
	 */
	ofw_discardmappings(proc0_pt_kernel.pv_va,
	    proc0_pt_sys.pv_va, PT_SIZE);
	ofw_discardmappings(proc0_pt_kernel.pv_va,
	    proc0_pt_kernel.pv_va, PT_SIZE);
	for (i = 0; i < KERNEL_VMDATA_PTS; i++)
		ofw_discardmappings(proc0_pt_kernel.pv_va,
		    proc0_pt_vmdata[i].pv_va, PT_SIZE);
	for (i = 0; i < KERNEL_OFW_PTS; i++)
		ofw_discardmappings(proc0_pt_kernel.pv_va,
		    proc0_pt_ofw[i].pv_va, PT_SIZE);
	for (i = 0; i < KERNEL_IO_PTS; i++)
		ofw_discardmappings(proc0_pt_kernel.pv_va,
		    proc0_pt_io[i].pv_va, PT_SIZE);
	ofw_discardmappings(proc0_pt_kernel.pv_va,
	    msgbuf.pv_va, MSGBUFSIZE);

	/*
	 * We did not throw away the proc0_pt_pte and proc0_pagedir
	 * mappings as well still want them. However we don't want them
	 * cached ...
	 * Really these should be uncached when allocated.
	 */
	pmap_map_entry(L1pagetable, proc0_pt_pte.pv_va,
	    proc0_pt_pte.pv_pa, VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);
	for (i = 0; i < (PD_SIZE / NBPG); ++i)
		pmap_map_entry(L1pagetable,
		    proc0_pagedir.pv_va + NBPG * i,
		    proc0_pagedir.pv_pa + NBPG * i,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);

	/*
	 * Construct the proc0 L2 pagetables that map page tables.
	 */

	/* Map entries in the L2pagetable used to map L2PTs. */
	pmap_map_entry(L1pagetable,
	    PROCESS_PAGE_TBLS_BASE + (0x00000000 >> (PGSHIFT-2)),
	    proc0_pt_sys.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);
	pmap_map_entry(L1pagetable,
	    PROCESS_PAGE_TBLS_BASE + (KERNEL_BASE >> (PGSHIFT-2)),
	    proc0_pt_kernel.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);
	pmap_map_entry(L1pagetable,
	    PROCESS_PAGE_TBLS_BASE + (PROCESS_PAGE_TBLS_BASE >> (PGSHIFT-2)),
	    proc0_pt_pte.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);
	for (i = 0; i < KERNEL_VMDATA_PTS; i++)
		pmap_map_entry(L1pagetable,
		    PROCESS_PAGE_TBLS_BASE + ((KERNEL_VM_BASE + i * 0x00400000)
		    >> (PGSHIFT-2)), proc0_pt_vmdata[i].pv_pa,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);
	for (i = 0; i < KERNEL_OFW_PTS; i++)
		pmap_map_entry(L1pagetable,
		    PROCESS_PAGE_TBLS_BASE + ((OFW_VIRT_BASE + i * 0x00400000)
		    >> (PGSHIFT-2)), proc0_pt_ofw[i].pv_pa,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);
	for (i = 0; i < KERNEL_IO_PTS; i++)
		pmap_map_entry(L1pagetable,
		    PROCESS_PAGE_TBLS_BASE + ((IO_VIRT_BASE + i * 0x00400000)
		    >> (PGSHIFT-2)), proc0_pt_io[i].pv_pa,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);

	/* update the top of the kernel VM */
	pmap_curmaxkvaddr = KERNEL_VM_BASE + ((KERNEL_VMDATA_PTS) * 0x00400000) - 1;
	
	/* 
         * gross hack for the sake of not thrashing the TLB and making
	 * cache flush more efficient: blast l1 ptes for sections.
         */
	for (oft = 0, tp = OFtranslations; oft < nOFtranslations; oft++, tp++) {
		vm_offset_t va = tp->virt;
		vm_offset_t pa = tp->phys;

		if (((va & (NBPD - 1)) == 0) && ((pa & (NBPD - 1)) == 0)) {
			int nsections = tp->size / NBPD;

			while (nsections--) {
				/* XXXJRT prot?? */
				pmap_map_section(L1pagetable, va, pa,
				    VM_PROT_READ|VM_PROT_WRITE,
				    (tp->mode & 0xC) == 0xC ? PTE_CACHE
							    : PTE_NOCACHE);
				va += NBPD;
				pa += NBPD;
			}
		}
	}

	/* OUT parameters are the new ttbbase and the pt which maps pts. */
	*proc0_ttbbase = proc0_pagedir;
	*proc0_ptpt = proc0_pt_pte;
}


static void
ofw_getphysmeminfo()
{
	int phandle;
	int mem_len;
	int avail_len;
	int i;

	if ((phandle = OF_finddevice("/memory")) == -1 ||
	    (mem_len = OF_getproplen(phandle, "reg")) <= 0 ||
	    (OFphysmem = (struct mem_region *)ofw_malloc(mem_len)) == 0 ||
	    OF_getprop(phandle, "reg", OFphysmem, mem_len) != mem_len ||
	    (avail_len = OF_getproplen(phandle, "available")) <= 0 ||
 	    (OFphysavail = (struct mem_region *)ofw_malloc(avail_len)) == 0 ||
	    OF_getprop(phandle, "available", OFphysavail, avail_len)
	    != avail_len)
		panic("can't get physmeminfo from OFW");

	nOFphysmem = mem_len / sizeof(struct mem_region);
	nOFphysavail = avail_len / sizeof(struct mem_region);

	/*
	 * Sort the blocks in each array into ascending address order.
	 * Also, page-align all blocks.
	 */
	for (i = 0; i < 2; i++) {
		struct mem_region *tmp = (i == 0) ? OFphysmem : OFphysavail;
		struct mem_region *mp;
		int cnt =  (i == 0) ? nOFphysmem : nOFphysavail;
		int j;

#ifdef	OLDPRINTFS
		printf("ofw_getphysmeminfo:  %d blocks\n", cnt);
#endif

		/* XXX - Convert all the values to host order. -JJK */
		for (j = 0, mp = tmp; j < cnt; j++, mp++) {
			mp->start = of_decode_int((unsigned char *)&mp->start);
			mp->size = of_decode_int((unsigned char *)&mp->size);
		}

		for (j = 0, mp = tmp; j < cnt; j++, mp++) {
			u_int s, sz;
			struct mem_region *mp1;

			/* Page-align start of the block. */
			s = mp->start % NBPG;
			if (s != 0) {
				s = (NBPG - s);

				if (mp->size >= s) {
					mp->start += s;
					mp->size -= s;
				}
			}

			/* Page-align the size. */
			mp->size -= mp->size % NBPG;

			/* Handle empty block. */
			if (mp->size == 0) {
				bcopy(mp + 1, mp, (cnt - (mp - tmp))
				    * sizeof(struct mem_region));
				cnt--;
				mp--;
				continue;
			}

			/* Bubble sort. */
			s = mp->start;
			sz = mp->size;
			for (mp1 = tmp; mp1 < mp; mp1++)
				if (s < mp1->start)
					break;
			if (mp1 < mp) {
				bcopy(mp1, mp1 + 1, (char *)mp - (char *)mp1);
				mp1->start = s;
				mp1->size = sz;
			}
		}

#ifdef	OLDPRINTFS
		for (mp = tmp; mp->size; mp++) {
			printf("%x, %x\n", mp->start, mp->size);
		}
#endif
	}
}


static void
ofw_getvirttranslations(void)
{
	int mmu_phandle;
	int mmu_ihandle;
	int trans_len;
	int over, len;
	int i;
	struct mem_translation *tp;

	mmu_ihandle = ofw_mmu_ihandle();

	/* overallocate to avoid increases during allocation */
	over = 4 * sizeof(struct mem_translation);
	if ((mmu_phandle = OF_instance_to_package(mmu_ihandle)) == -1 ||
	    (len = OF_getproplen(mmu_phandle, "translations")) <= 0 ||
	    (OFtranslations = ofw_malloc(len + over)) == 0 ||
	    (trans_len = OF_getprop(mmu_phandle, "translations",
	    OFtranslations, len + over)) > (len + over))
		panic("can't get virttranslations from OFW");

	/* XXX - Convert all the values to host order. -JJK */
	nOFtranslations = trans_len / sizeof(struct mem_translation);
#ifdef	OLDPRINTFS
	printf("ofw_getvirtmeminfo:  %d blocks\n", nOFtranslations);
#endif
	for (i = 0, tp = OFtranslations; i < nOFtranslations; i++, tp++) {
		tp->virt = of_decode_int((unsigned char *)&tp->virt);
		tp->size = of_decode_int((unsigned char *)&tp->size);
		tp->phys = of_decode_int((unsigned char *)&tp->phys);
		tp->mode = of_decode_int((unsigned char *)&tp->mode);
	}
}

/*
 * ofw_valloc: allocate blocks of VM for IO and other special purposes
 */
typedef struct _vfree {
	struct _vfree *pNext;
	vm_offset_t start;
	vm_size_t   size;
} VFREE, *PVFREE;

static VFREE vfinitial = { NULL, IO_VIRT_BASE, IO_VIRT_SIZE };

static PVFREE vflist = &vfinitial;

static vm_offset_t
ofw_valloc(size, align)
	vm_offset_t size;
	vm_offset_t align;
{
	PVFREE        *ppvf;
	PVFREE        pNew;
	vm_offset_t   new;
	vm_offset_t   lead;

	for (ppvf = &vflist; *ppvf; ppvf = &((*ppvf)->pNext)) {
		if (align == 0) {
			new = (*ppvf)->start;
			lead = 0;
		} else {
			new  = ((*ppvf)->start + (align - 1)) & ~(align - 1);
			lead = new - (*ppvf)->start;
		}

		if (((*ppvf)->size - lead) >= size) {
 			if (lead == 0) {
				/* using whole block */
				if (size == (*ppvf)->size) { 
					/* splice out of list */
					(*ppvf) = (*ppvf)->pNext;
				} else { /* tail of block is free */
					(*ppvf)->start = new + size;
					(*ppvf)->size -= size;
				}
			} else {
				vm_size_t tail = ((*ppvf)->start
				    + (*ppvf)->size) - (new + size);
				/* free space at beginning */
				(*ppvf)->size = lead;

				if (tail != 0) {
					/* free space at tail */
					pNew = ofw_malloc(sizeof(VFREE));
					pNew->pNext  = (*ppvf)->pNext;
					(*ppvf)->pNext = pNew;
					pNew->start  = new + size;
					pNew->size   = tail;
				}
			}
			return new;
		} /* END if */
	} /* END for */

	return -1;
}

vm_offset_t
ofw_map(pa, size, cb_bits)
	vm_offset_t pa;
	vm_size_t size;
	int cb_bits;
{
	vm_offset_t va;

	if ((va = ofw_valloc(size, size)) == -1)
		panic("cannot alloc virtual memory for %#lx", pa);

	ofw_claimvirt(va, size, 0); /* make sure OFW knows about the memory */

	ofw_settranslation(va, pa, size, PT_AP(AP_KRW) | cb_bits);

	return va;
}

static int
ofw_mem_ihandle(void)
{
	static int mem_ihandle = 0;
	int chosen;

	if (mem_ihandle != 0)
		return(mem_ihandle);

	if ((chosen = OF_finddevice("/chosen")) == -1 ||
	    OF_getprop(chosen, "memory", &mem_ihandle, sizeof(int)) < 0)
		panic("ofw_mem_ihandle");

	mem_ihandle = of_decode_int((unsigned char *)&mem_ihandle);

	return(mem_ihandle);
}


static int
ofw_mmu_ihandle(void)
{
	static int mmu_ihandle = 0;
	int chosen;

	if (mmu_ihandle != 0)
		return(mmu_ihandle);

	if ((chosen = OF_finddevice("/chosen")) == -1 ||
	    OF_getprop(chosen, "mmu", &mmu_ihandle, sizeof(int)) < 0)
		panic("ofw_mmu_ihandle");

	mmu_ihandle = of_decode_int((unsigned char *)&mmu_ihandle);

	return(mmu_ihandle);
}


/* Return -1 on failure. */
static vm_offset_t
ofw_claimphys(pa, size, align)
	vm_offset_t pa;
	vm_size_t size;
	vm_offset_t align;
{
	int mem_ihandle = ofw_mem_ihandle();

/*	printf("ofw_claimphys (%x, %x, %x) --> ", pa, size, align);*/
	if (align == 0) {
		/* Allocate at specified base; alignment is ignored. */
		pa = OF_call_method_1("claim", mem_ihandle, 3, pa, size, align);
	} else {
		/* Allocate anywhere, with specified alignment. */
		pa = OF_call_method_1("claim", mem_ihandle, 2, size, align);
	}

/*	printf("%x\n", pa);*/
	return(pa);
}


#if 0
/* Return -1 on failure. */
static vm_offset_t
ofw_releasephys(pa, size)
	vm_offset_t pa;
	vm_size_t size;
{
	int mem_ihandle = ofw_mem_ihandle();

/*	printf("ofw_releasephys (%x, %x)\n", pa, size);*/

	return (OF_call_method_1("release", mem_ihandle, 2, pa, size));
}
#endif

/* Return -1 on failure. */
static vm_offset_t
ofw_claimvirt(va, size, align)
	vm_offset_t va;
	vm_size_t size;
	vm_offset_t align;
{
	int mmu_ihandle = ofw_mmu_ihandle();

	/*printf("ofw_claimvirt (%x, %x, %x) --> ", va, size, align);*/
	if (align == 0) {
		/* Allocate at specified base; alignment is ignored. */
		va = OF_call_method_1("claim", mmu_ihandle, 3, va, size, align);
	} else {
		/* Allocate anywhere, with specified alignment. */
		va = OF_call_method_1("claim", mmu_ihandle, 2, size, align);
	}

	/*printf("%x\n", va);*/
	return(va);
}


/* Return -1 if no mapping. */
vm_offset_t
ofw_gettranslation(va)
	vm_offset_t va;
{
	int mmu_ihandle = ofw_mmu_ihandle();
	vm_offset_t pa;
	int mode;
	int exists;

	/*printf("ofw_gettranslation (%x) --> ", va);*/
	exists = 0;	    /* gets set to true if translation exists */
	if (OF_call_method("translate", mmu_ihandle, 1, 3, va, &pa, &mode,
	    &exists) != 0)
		return(-1);

	/*printf("%x\n", exists ? pa : -1);*/
	return(exists ? pa : -1);
}


static void
ofw_settranslation(va, pa, size, mode)
	vm_offset_t va;
	vm_offset_t pa;
	vm_size_t size;
	int mode;
{
	int mmu_ihandle = ofw_mmu_ihandle();

/*printf("ofw_settranslation (%x, %x, %x, %x) --> void", va, pa, size, mode);*/
	if (OF_call_method("map", mmu_ihandle, 4, 0, pa, va, size, mode) != 0)
		panic("ofw_settranslation failed");
}

/*
 *  Allocation routine used before the kernel takes over memory.
 *  Use this for efficient storage for things that aren't rounded to
 *  page size.
 *
 *  The point here is not necessarily to be very efficient (even though
 *  that's sort of nice), but to do proper dynamic allocation to avoid
 *  size-limitation errors.
 *
 */

typedef struct _leftover {
	struct _leftover *pNext;
	vm_size_t size;
} LEFTOVER, *PLEFTOVER;

/* leftover bits of pages.  first word is pointer to next.
   second word is size of leftover */
static PLEFTOVER leftovers = NULL;

static void *
ofw_malloc(size)
	vm_size_t size;
{
	PLEFTOVER   *ppLeftover;
	PLEFTOVER   pLeft;
	pv_addr_t   new;
	vm_size_t   newSize, claim_size;

	/* round and set minimum size */
	size = max(sizeof(LEFTOVER), 
	    ((size + (sizeof(LEFTOVER) - 1)) & ~(sizeof(LEFTOVER) - 1)));

	for (ppLeftover = &leftovers; *ppLeftover;
	    ppLeftover = &((*ppLeftover)->pNext))
		if ((*ppLeftover)->size >= size)
			break;

	if (*ppLeftover) { /* have a leftover of the right size */
		/* remember the leftover */
		new.pv_va = (vm_offset_t)*ppLeftover;
		if ((*ppLeftover)->size < (size + sizeof(LEFTOVER))) {
			/* splice out of chain */
			*ppLeftover = (*ppLeftover)->pNext;
		} else {
			/* remember the next pointer */
			pLeft = (*ppLeftover)->pNext;
			newSize = (*ppLeftover)->size - size; /* reduce size */
			/* move pointer */
			*ppLeftover = (PLEFTOVER)(((vm_offset_t)*ppLeftover)
			    + size); 
			(*ppLeftover)->pNext = pLeft;
			(*ppLeftover)->size  = newSize;
		}
	} else {
		claim_size = (size + NBPG - 1) & ~(NBPG - 1);
		ofw_claimpages(&virt_freeptr, &new, claim_size);
		if ((size + sizeof(LEFTOVER)) <= claim_size) {
			pLeft = (PLEFTOVER)(new.pv_va + size);
			pLeft->pNext = leftovers;
			pLeft->size = claim_size - size;
			leftovers = pLeft;
		}
	}

	return (void *)(new.pv_va);
}

/*
 *  Here is a really, really sleazy free.  It's not used right now,
 *  because it's not worth the extra complexity for just a few bytes.
 *
 */
#if 0
static void
ofw_free(addr, size)
	vm_offset_t addr;
	vm_size_t size;
{
	PLEFTOVER pLeftover = (PLEFTOVER)addr;

	/* splice right into list without checks or compaction */
	pLeftover->pNext = leftovers;
	pLeftover->size  = size;
	leftovers        = pLeftover;
}
#endif

/*
 *  Allocate and zero round(size)/NBPG pages of memory.
 *  We guarantee that the allocated memory will be
 *  aligned to a boundary equal to the smallest power of
 *  2 greater than or equal to size.
 *  free_pp is an IN/OUT parameter which points to the
 *  last allocated virtual address in an allocate-downwards
 *  stack.  pv_p is an OUT parameter which contains the
 *  virtual and physical base addresses of the allocated
 *  memory.
 */
static void
ofw_claimpages(free_pp, pv_p, size)
	vm_offset_t *free_pp;
	pv_addr_t *pv_p;
	vm_size_t size;
{
	/* round-up to page boundary */
	vm_size_t alloc_size = (size + NBPG - 1) & ~(NBPG - 1);
	vm_size_t aligned_size;
	vm_offset_t va, pa;

	if (alloc_size == 0)
		panic("ofw_claimpages zero");

	for (aligned_size = 1; aligned_size < alloc_size; aligned_size <<= 1)
		;

	/*  The only way to provide the alignment guarantees is to
	 *  allocate the virtual and physical ranges separately,
	 *  then do an explicit map call.
	 */
	va = (*free_pp & ~(aligned_size - 1)) - aligned_size;
	if (ofw_claimvirt(va, alloc_size, 0) != va)
		panic("ofw_claimpages va alloc");
	pa = ofw_claimphys(0, alloc_size, aligned_size);
	if (pa == -1)
		panic("ofw_claimpages pa alloc");
	/* XXX - what mode? -JJK */
	ofw_settranslation(va, pa, alloc_size, -1);

	/* The memory's mapped-in now, so we can zero it. */
	bzero((char *)va, alloc_size);

	/* Set OUT parameters. */
	*free_pp = va;
	pv_p->pv_va = va;
	pv_p->pv_pa = pa;
}


static void
ofw_discardmappings(L2pagetable, va, size)
	vm_offset_t L2pagetable;
	vm_offset_t va;
	vm_size_t size;
{
	/* round-up to page boundary */
	vm_size_t alloc_size = (size + NBPG - 1) & ~(NBPG - 1);
	int npages = alloc_size / NBPG;

	if (npages == 0)
		panic("ofw_discardmappings zero");

	/* Discard each mapping. */
	for (; npages > 0; va += NBPG, npages--) {
		/* Sanity. The current entry should be non-null. */
		if (ReadWord(L2pagetable + ((va >> 10) & 0x00000FFC)) == 0)
			panic("ofw_discardmappings zero entry");

		/* Clear the entry. */
		WriteWord(L2pagetable + ((va >> 10) & 0x00000FFC), 0);
	}
}


static void
ofw_initallocator(void)
{
    
}

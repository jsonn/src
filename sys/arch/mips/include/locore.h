/*	$NetBSD: locore.h,v 1.14.2.1 1998/11/19 06:44:50 nisimura Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

/*
 * Jump table for MIPS cpu locore functions that are implemented
 * differently on different generations, or instruction-level
 * archtecture (ISA) level, the Mips family.
 * The following functions must be provided for each mips ISA level:
 *
 * 
 *	MachConfigCache
 *	MachFlushCache
 *	MachFlushDCache
 *	MachFlushICache
 *	MachForceCacheUpdate
 *	MachSetPID
 *	MachTLBFlush
 *	MachTLBFlushAddr __P()
 *	MachTLBUpdate (u_int, (pt_entry_t?) u_int);
 *	wbflush
 *	proc_trampoline()
 *	switch_exit()
 *	cpu_switch_resume()
 *
 * We currently provide support for:
 *
 *	r2000 and r3000 (mips ISA-I)
 *	r4000 and r4400 in 32-bit mode (mips ISA-III?)
 */

#ifndef _MIPS_LOCORE_H
#define  _MIPS_LOCORE_H

/*
 * locore service routine for exeception vectors. Used outside locore
 * only to print them by name in stack tracebacks
 */

/* Block out one hardware interrupt-enable bit. */
extern int	Mach_spl0 __P((void)), Mach_spl1 __P((void));
extern int	Mach_spl2 __P((void)), Mach_spl3 __P((void));
extern int	Mach_spl4 __P((void)), Mach_spl5 __P((void));

/* Block out nested interrupt-enable bits. */
extern int	cpu_spl0 __P((void)), cpu_spl1 __P((void));
extern int	cpu_spl2 __P((void)), cpu_spl3 __P((void));
extern int	cpu_spl4 __P((void)), cpu_spl5 __P((void));
extern int	splhigh __P((void));

extern u_int32_t mips_read_causereg __P((void));
extern u_int32_t mips_read_statusreg __P((void));

extern void mips1_ConfigCache  __P((void));
extern void mips1_FlushCache  __P((void));
extern void mips1_FlushDCache  __P((vaddr_t addr, vsize_t len));
extern void mips1_FlushICache  __P((vaddr_t addr, vsize_t len));
extern void mips1_ForceCacheUpdate __P((void));
extern void mips1_SetPID   __P((int pid));
extern void mips1_TLBFlush __P((int numtlb));
extern void mips1_TLBFlushAddr   __P( /* XXX Really pte highpart ? */
					  (vaddr_t addr));
extern int mips1_TLBUpdate __P((u_int, /*pt_entry_t*/ u_int));
extern void mips1_TLBWriteIndexed  __P((u_int index, u_int high,
					    u_int low));
extern void mips1_wbflush __P((void));
extern void mips1_proc_trampoline __P((void));
extern void mips1_switch_exit __P((struct proc *));
extern void mips1_cpu_switch_resume __P((void));

extern void mips3_ConfigCache __P((void));
extern void mips3_FlushCache  __P((void));
extern void mips3_FlushDCache __P((vaddr_t addr, vaddr_t len));
extern void mips3_FlushICache __P((vaddr_t addr, vaddr_t len));
extern void mips3_ForceCacheUpdate __P((void));
extern void mips3_HitFlushDCache __P((vaddr_t, int));
extern void mips3_SetPID  __P((int pid));
extern void mips3_TLBFlush __P((int numtlb));
extern void mips3_TLBFlushAddr __P( /* XXX Really pte highpart ? */
					  (vaddr_t addr));
extern int mips3_TLBUpdate __P((u_int, /*pt_entry_t*/ u_int));
struct tlb;
extern void mips3_TLBRead __P((int, struct tlb *));
extern void mips3_TLBWriteIndexedVPS __P((u_int index, struct tlb *tlb));
extern void mips3_TLBWriteIndexed __P((u_int index, u_int high,
					   u_int lo0, u_int lo1));
extern void mips3_wbflush __P((void));
extern void mips3_proc_trampoline __P((void));
extern void mips3_switch_exit __P((struct proc *));
extern void mips3_cpu_switch_resume __P((void));

extern void mips3_SetWIRED __P((int));

extern u_int32_t mips3_cycle_count __P((void));
extern u_int32_t mips3_read_compare __P((void));
extern u_int32_t mips3_read_config __P((void));
extern void mips3_write_compare __P((u_int32_t));

/*
 *  A vector with an entry for each mips-ISA-level dependent
 * locore function, and macros which jump through it.
 * XXX the macro names are chosen to be compatible with the old
 * Sprite  coding-convention names used in 4.4bsd/pmax.
 */
typedef struct  {
	void (*configCache) __P((void));
	void (*flushCache)  __P((void));
	void (*flushDCache) __P((vaddr_t addr, vsize_t len));
	void (*flushICache) __P((vaddr_t addr, vsize_t len));
	void (*forceCacheUpdate)  __P((void));
	void (*setTLBpid)  __P((int pid));
	void (*tlbFlush)  __P((int numtlb));
	void (*tlbFlushAddr)  __P((vaddr_t)); /* XXX Really pte highpart ? */
	int (*tlbUpdate)  __P((u_int highreg, u_int lowreg));
	void (*wbflush) __P((void));
	void (*proc_trampoline) __P((void));
	void (*mips_switch_exit) __P((struct proc *));
	void (*cpu_switch_resume) __P((void));
} mips_locore_jumpvec_t;

/* Override writebuffer-drain method. */
void mips_set_wbflush __P((void (*) __P((void)) ));


/*
 * The "active" locore-fuction vector, and

 */
extern mips_locore_jumpvec_t mips_locore_jumpvec;
extern mips_locore_jumpvec_t r2000_locore_vec;
extern mips_locore_jumpvec_t r4000_locore_vec;

#if defined(MIPS3) && !defined (MIPS1)
#define MachConfigCache		mips3_ConfigCache
#define MachFlushCache		mips3_FlushCache
#define MachFlushDCache		mips3_FlushDCache
#define MachFlushICache		mips3_FlushICache
#define MachForceCacheUpdate	mips3_ForceCacheUpdate
#define MachSetPID		mips3_SetPID
#define MachTLBFlush()		mips3_TLBFlush(mips_num_tlb_entries)
#define MachTLBFlushAddr	mips3_TLBFlushAddr
#define MachTLBUpdate		mips3_TLBUpdate
#define wbflush			mips3_wbflush
#define proc_trampoline		mips3_proc_trampoline
#define switch_exit		mips3_switch_exit
#endif

#if !defined(MIPS3) && defined (MIPS1)
#define MachConfigCache		mips1_ConfigCache
#define MachFlushCache		mips1_FlushCache
#define MachFlushDCache		mips1_FlushDCache
#define MachFlushICache		mips1_FlushICache
#define MachForceCacheUpdate	mips1_ForceCacheUpdate
#define MachSetPID		mips1_SetPID
#define MachTLBFlush()		mips1_TLBFlush(MIPS1_TLB_NUM_TLB_ENTRIES)
#define MachTLBFlushAddr	mips1_TLBFlushAddr
#define MachTLBUpdate		mips1_TLBUpdate
#define wbflush			mips1_wbflush
#define proc_trampoline		mips1_proc_trampoline
#define switch_exit		mips1_switch_exit
#endif



#if defined(MIPS3) && defined (MIPS1)
#define MachConfigCache		(*(mips_locore_jumpvec.configCache))
#define MachFlushCache		(*(mips_locore_jumpvec.flushCache))
#define MachFlushDCache		(*(mips_locore_jumpvec.flushDCache))
#define MachFlushICache		(*(mips_locore_jumpvec.flushICache))
#define MachForceCacheUpdate	(*(mips_locore_jumpvec.forceCacheUpdate))
#define MachSetPID		(*(mips_locore_jumpvec.setTLBpid))
#define MachTLBFlush()		(*(mips_locore_jumpvec.tlbFlush))(mips_num_tlb_entries)
#define MachTLBFlushAddr	(*(mips_locore_jumpvec.tlbFlushAddr))
#define MachTLBUpdate		(*(mips_locore_jumpvec.tlbUpdate))
#define wbflush			(*(mips_locore_jumpvec.wbflush))
#define proc_trampoline		(mips_locore_jumpvec.proc_trampoline)
#define switch_exit		(*(mips_locore_jumpvec.mips_switch_exit))
#endif

/* cpu_switch_resume not called directly */


/*
 * CPU identification, from PRID register.
 */
union cpuprid {
	int	cpuprid;
	struct {
#if BYTE_ORDER == BIG_ENDIAN
		u_int	pad1:16;	/* reserved */
		u_int	cp_imp:8;	/* implementation identifier */
		u_int	cp_majrev:4;	/* major revision identifier */
		u_int	cp_minrev:4;	/* minor revision identifier */
#else
		u_int	cp_minrev:4;	/* minor revision identifier */
		u_int	cp_majrev:4;	/* major revision identifier */
		u_int	cp_imp:8;	/* implementation identifier */
		u_int	pad1:16;	/* reserved */
#endif
	} cpu;
};


#ifdef _KERNEL

/*
 * Global variables used to communicate CPU type, and parameters
 * such as cache size, from locore to higher-level code (e.g., pmap).
 */
extern union	cpuprid cpu_id;
extern union	cpuprid fpu_id;
extern int	cpu_arch;
extern int	mips_num_tlb_entries;
extern u_int	mips_L1DCacheSize;
extern u_int	mips_L1ICacheSize;
extern u_int	mips_L1DCacheLSize;
extern u_int	mips_L1ICacheLSize;
extern int	mips_L2CachePresent;
extern u_int	mips_L2CacheLSize;
extern u_int	mips_CacheAliasMask;
extern	struct intr_tab intr_tab[];

#ifdef MIPS3
extern int	mips3_L1TwoWayCache;
extern int	mips3_cacheflush_bug;
#endif /* MIPS3 */

#endif

#endif	/* _MIPS_LOCORE_H */

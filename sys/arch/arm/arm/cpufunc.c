/*	$NetBSD: cpufunc.c,v 1.15.2.2 2002/01/08 00:23:06 nathanw Exp $	*/

/*
 * arm7tdmi support code Copyright (c) 2001 John Fremlin
 * arm8 support code Copyright (c) 1997 ARM Limited
 * arm8 support code Copyright (c) 1997 Causality Limited
 * arm9 support code Copyright (C) 2001 ARM Ltd
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 *	This product includes software developed by Causality Limited.
 * 4. The name of Causality Limited may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUSALITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpufuncs.c
 *
 * C functions for supporting CPU / MMU / TLB specific operations.
 *
 * Created      : 30/01/97
 */

#include "opt_compat_netbsd.h"
#include "opt_cputypes.h"
#include "opt_pmap_debug.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <machine/cpu.h>
#include <machine/bootconfig.h>
#include <arch/arm/arm/disassem.h>

#include <arm/cpufunc.h>

#ifdef CPU_XSCALE
#include <arm/xscale/i80200reg.h>
#endif

/* PRIMARY CACHE VARIABLES */
int	arm_picache_size;
int	arm_picache_line_size;
int	arm_picache_ways;

int	arm_pdcache_size;	/* and unified */
int	arm_pdcache_line_size;
int	arm_pdcache_ways;

int	arm_pcache_type;
int	arm_pcache_unified;

int	arm_dcache_align;
int	arm_dcache_align_mask;

#ifdef CPU_ARM3
struct cpu_functions arm3_cpufuncs = {
	/* CPU functions */
	
	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	arm3_control,			/* control		*/
	NULL,				/* domain		*/
	NULL,				/* setttb		*/
	NULL,				/* faultstatus		*/
	NULL,				/* faultaddress		*/

	/* TLB functions */

	cpufunc_nullop,			/* tlb_flushID		*/
	(void *)cpufunc_nullop,		/* tlb_flushID_SE	*/
	cpufunc_nullop,			/* tlb_flushI		*/
	(void *)cpufunc_nullop,		/* tlb_flushI_SE	*/
	cpufunc_nullop,			/* tlb_flushD		*/
	(void *)cpufunc_nullop,		/* tlb_flushD_SE	*/

	/* Cache functions */

	arm3_cache_flush,		/* cache_flushID	*/
	(void *)arm3_cache_flush,	/* cache_flushID_SE	*/
	arm3_cache_flush,		/* cache_flushI		*/
	(void *)arm3_cache_flush,	/* cache_flushI_SE	*/
	arm3_cache_flush,		/* cache_flushD		*/
	(void *)arm3_cache_flush,	/* cache_flushD_SE	*/

	cpufunc_nullop,			/* cache_cleanID	s*/
	(void *)cpufunc_nullop,		/* cache_cleanID_E	s*/
	cpufunc_nullop,			/* cache_cleanD		s*/
	(void *)cpufunc_nullop,		/* cache_cleanD_E	*/

	arm3_cache_flush,		/* cache_purgeID	s*/
	(void *)arm3_cache_flush,	/* cache_purgeID_E	s*/
	arm3_cache_flush,		/* cache_purgeD		s*/
	(void *)arm3_cache_flush,	/* cache_purgeD_E	s*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	cpufunc_nullop,			/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	cpufunc_nullop,			/* cache_syncI		*/
	(void *)cpufunc_nullop,		/* cache_cleanID_rng	*/
	(void *)cpufunc_nullop,		/* cache_cleanD_rng	*/
	(void *)arm3_cache_flush,	/* cache_purgeID_rng	*/
	(void *)arm3_cache_flush,	/* cache_purgeD_rng	*/
	(void *)cpufunc_nullop,		/* cache_syncI_rng	*/

	early_abort_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	NULL,				/* context_switch	*/

	(void *)cpufunc_nullop		/* cpu setup		*/

};
#endif	/* CPU_ARM3 */

#ifdef CPU_ARM6
struct cpu_functions arm6_cpufuncs = {
	/* CPU functions */
	
	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/
 
	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	arm67_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	arm67_tlb_flush,		/* tlb_flushID		*/
	arm67_tlb_purge,		/* tlb_flushID_SE	*/
	arm67_tlb_flush,		/* tlb_flushI		*/
	arm67_tlb_purge,		/* tlb_flushI_SE	*/
	arm67_tlb_flush,		/* tlb_flushD		*/
	arm67_tlb_purge,		/* tlb_flushD_SE	*/

	/* Cache functions */

	arm67_cache_flush,		/* cache_flushID	*/
	(void *)arm67_cache_flush,	/* cache_flushID_SE	*/
	arm67_cache_flush,		/* cache_flushI		*/
	(void *)arm67_cache_flush,	/* cache_flushI_SE	*/
	arm67_cache_flush,		/* cache_flushD		*/
	(void *)arm67_cache_flush,	/* cache_flushD_SE	*/

	cpufunc_nullop,			/* cache_cleanID	s*/
	(void *)cpufunc_nullop,		/* cache_cleanID_E	s*/
	cpufunc_nullop,			/* cache_cleanD		s*/
	(void *)cpufunc_nullop,		/* cache_cleanD_E	*/

	arm67_cache_flush,		/* cache_purgeID	s*/
	(void *)arm67_cache_flush,	/* cache_purgeID_E	s*/
	arm67_cache_flush,		/* cache_purgeD		s*/
	(void *)arm67_cache_flush,	/* cache_purgeD_E	s*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	cpufunc_nullop,			/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	cpufunc_nullop,			/* cache_syncI		*/
	(void *)cpufunc_nullop,		/* cache_cleanID_rng	*/
	(void *)cpufunc_nullop,		/* cache_cleanD_rng	*/
	(void *)arm67_cache_flush,	/* cache_purgeID_rng	*/
	(void *)arm67_cache_flush,	/* cache_purgeD_rng	*/
	(void *)cpufunc_nullop,		/* cache_syncI_rng	*/

#ifdef ARM6_LATE_ABORT
	late_abort_fixup,		/* dataabt_fixup	*/
#else
	early_abort_fixup,		/* dataabt_fixup	*/
#endif
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	arm67_context_switch,		/* context_switch	*/

	arm6_setup			/* cpu setup		*/

};
#endif	/* CPU_ARM6 */

#ifdef CPU_ARM7
struct cpu_functions arm7_cpufuncs = {
	/* CPU functions */
	
	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	arm67_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	arm67_tlb_flush,		/* tlb_flushID		*/
	arm67_tlb_purge,		/* tlb_flushID_SE	*/
	arm67_tlb_flush,		/* tlb_flushI		*/
	arm67_tlb_purge,		/* tlb_flushI_SE	*/
	arm67_tlb_flush,		/* tlb_flushD		*/
	arm67_tlb_purge,		/* tlb_flushD_SE	*/

	/* Cache functions */

	arm67_cache_flush,		/* cache_flushID	*/
	(void *)arm67_cache_flush,	/* cache_flushID_SE	*/
	arm67_cache_flush,		/* cache_flushI		*/
	(void *)arm67_cache_flush,	/* cache_flushI_SE	*/
	arm67_cache_flush,		/* cache_flushD		*/
	(void *)arm67_cache_flush,	/* cache_flushD_SE	*/

	cpufunc_nullop,			/* cache_cleanID	s*/
	(void *)cpufunc_nullop,		/* cache_cleanID_E	s*/
	cpufunc_nullop,			/* cache_cleanD		s*/
	(void *)cpufunc_nullop,		/* cache_cleanD_E	*/

	arm67_cache_flush,		/* cache_purgeID	s*/
	(void *)arm67_cache_flush,	/* cache_purgeID_E	s*/
	arm67_cache_flush,		/* cache_purgeD		s*/
	(void *)arm67_cache_flush,	/* cache_purgeD_E	s*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	cpufunc_nullop,			/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	cpufunc_nullop,			/* cache_syncI		*/
	(void *)cpufunc_nullop,		/* cache_cleanID_rng	*/
	(void *)cpufunc_nullop,		/* cache_cleanD_rng	*/
	(void *)arm67_cache_flush,	/* cache_purgeID_rng	*/
	(void *)arm67_cache_flush,	/* cache_purgeD_rng	*/
	(void *)cpufunc_nullop,		/* cache_syncI_rng	*/

	late_abort_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	arm67_context_switch,		/* context_switch	*/

	arm7_setup			/* cpu setup		*/

};
#endif	/* CPU_ARM7 */

#ifdef CPU_ARM7TDMI
struct cpu_functions arm7tdmi_cpufuncs = {
	/* CPU functions */
	
	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	arm7tdmi_setttb,		/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	arm7tdmi_tlb_flushID,		/* tlb_flushID		*/
	arm7tdmi_tlb_flushID_SE,	/* tlb_flushID_SE	*/
	arm7tdmi_tlb_flushID,		/* tlb_flushI		*/
	arm7tdmi_tlb_flushID_SE,	/* tlb_flushI_SE	*/
	arm7tdmi_tlb_flushID,		/* tlb_flushD		*/
	arm7tdmi_tlb_flushID_SE,	/* tlb_flushD_SE	*/

	/* Cache functions */

	arm7tdmi_cache_flushID,		/* cache_flushID	*/
	(void *)arm7tdmi_cache_flushID,	/* cache_flushID_SE	*/
	arm7tdmi_cache_flushID,		/* cache_flushI		*/
	(void *)arm7tdmi_cache_flushID,	/* cache_flushI_SE	*/
	arm7tdmi_cache_flushID,		/* cache_flushD		*/
	(void *)arm7tdmi_cache_flushID,	/* cache_flushD_SE	*/

	cpufunc_nullop,			/* cache_cleanID	s*/
	(void *)cpufunc_nullop,		/* cache_cleanID_E	s*/
	cpufunc_nullop,			/* cache_cleanD		s*/
	(void *)cpufunc_nullop,		/* cache_cleanD_E	*/

	arm7tdmi_cache_flushID,		/* cache_purgeID	s*/
	(void *)arm7tdmi_cache_flushID,	/* cache_purgeID_E	s*/
	arm7tdmi_cache_flushID,		/* cache_purgeD		s*/
	(void *)arm7tdmi_cache_flushID,	/* cache_purgeD_E	s*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	cpufunc_nullop,			/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	cpufunc_nullop,			/* cache_syncI		*/
	(void *)cpufunc_nullop,		/* cache_cleanID_rng	*/
	(void *)cpufunc_nullop,		/* cache_cleanD_rng	*/
	(void *)arm7tdmi_cache_flushID,	/* cache_purgeID_rng	*/
	(void *)arm7tdmi_cache_flushID,	/* cache_purgeD_rng	*/
	(void *)cpufunc_nullop,		/* cache_syncI_rng	*/

	late_abort_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	arm7tdmi_context_switch,	/* context_switch	*/

	arm7tdmi_setup			/* cpu setup		*/

};
#endif	/* CPU_ARM7TDMI */

#ifdef CPU_ARM8
struct cpu_functions arm8_cpufuncs = {
	/* CPU functions */
	
	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	arm8_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	arm8_tlb_flushID,		/* tlb_flushID		*/
	arm8_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	arm8_tlb_flushID,		/* tlb_flushI		*/
	arm8_tlb_flushID_SE,		/* tlb_flushI_SE	*/
	arm8_tlb_flushID,		/* tlb_flushD		*/
	arm8_tlb_flushID_SE,		/* tlb_flushD_SE	*/

	/* Cache functions */

	arm8_cache_flushID,		/* cache_flushID	*/
	arm8_cache_flushID_E,		/* cache_flushID_SE	*/
	arm8_cache_flushID,		/* cache_flushI		*/
	arm8_cache_flushID_E,		/* cache_flushI_SE	*/
	arm8_cache_flushID,		/* cache_flushD		*/
	arm8_cache_flushID_E,		/* cache_flushD_SE	*/

	arm8_cache_cleanID,		/* cache_cleanID	s*/
	arm8_cache_cleanID_E,		/* cache_cleanID_E	s*/
	arm8_cache_cleanID,		/* cache_cleanD		s*/
	arm8_cache_cleanID_E,		/* cache_cleanD_E	*/

	arm8_cache_purgeID,		/* cache_purgeID	s*/
	arm8_cache_purgeID_E,		/* cache_purgeID_E	s*/
	arm8_cache_purgeID,		/* cache_purgeD		s*/
	arm8_cache_purgeID_E,		/* cache_purgeD_E	s*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	cpufunc_nullop,			/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	(void *)cpufunc_nullop,		/* cache_syncI		*/
	(void *)arm8_cache_cleanID,	/* cache_cleanID_rng	*/
	(void *)arm8_cache_cleanID,	/* cache_cleanD_rng	*/
	(void *)arm8_cache_purgeID,	/* cache_purgeID_rng	*/
	(void *)arm8_cache_purgeID,	/* cache_purgeD_rng	*/
	(void *)cpufunc_nullop,		/* cache_syncI_rng	*/

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	arm8_context_switch,		/* context_switch	*/

	arm8_setup			/* cpu setup		*/
};          
#endif	/* CPU_ARM8 */

#ifdef CPU_ARM9
struct cpu_functions arm9_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	arm9_setttb,			/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	arm9_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache functions */

	arm9_cache_flushID,		/* cache_flushID	*/
	arm9_cache_flushID_SE,		/* cache_flushID_SE	*/
	arm9_cache_flushI,		/* cache_flushI		*/
	arm9_cache_flushI_SE,		/* cache_flushI_SE	*/
	arm9_cache_flushD,		/* cache_flushD		*/
	arm9_cache_flushD_SE,		/* cache_flushD_SE	*/

	/* ... lets use the cache in write-through mode.  */
	arm9_cache_cleanID,		/* cache_cleanID	*/
	(void *)arm9_cache_cleanID,	/* cache_cleanID_SE	*/
	arm9_cache_cleanID,		/* cache_cleanD		*/
	(void *)arm9_cache_cleanID,	/* cache_cleanD_SE	*/

	arm9_cache_flushID,		/* cache_purgeID	*/
	arm9_cache_flushID_SE,		/* cache_purgeID_SE	*/
	arm9_cache_flushD,		/* cache_purgeD		*/
	arm9_cache_flushD_SE,		/* cache_purgeD_SE	*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */
	arm9_cache_syncI,		/* cache_syncI		*/
	(void *)arm9_cache_cleanID,	/* cache_cleanID_rng	*/
	(void *)arm9_cache_cleanID,	/* cache_cleanD_rng	*/
	arm9_cache_flushID_rng,		/* cache_purgeID_rng	*/
	arm9_cache_flushD_rng,		/* cache_purgeD_rng	*/
	arm9_cache_syncI_rng,		/* cache_syncI_rng	*/

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	arm9_context_switch,		/* context_switch	*/

	arm9_setup			/* cpu setup		*/

};
#endif /* CPU_ARM9 */

#ifdef CPU_SA110
struct cpu_functions sa110_cpufuncs = {
	/* CPU functions */
	
	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	sa110_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	sa110_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache functions */

	sa110_cache_flushID,		/* cache_flushID	*/
	(void *)sa110_cache_flushID,	/* cache_flushID_SE	*/
	sa110_cache_flushI,		/* cache_flushI		*/
	(void *)sa110_cache_flushI,	/* cache_flushI_SE	*/
	sa110_cache_flushD,		/* cache_flushD		*/
	sa110_cache_flushD_SE,		/* cache_flushD_SE	*/

	sa110_cache_cleanID,		/* cache_cleanID	s*/
	sa110_cache_cleanD_E,		/* cache_cleanID_E	s*/
	sa110_cache_cleanD,		/* cache_cleanD		s*/
	sa110_cache_cleanD_E,		/* cache_cleanD_E	*/

	sa110_cache_purgeID,		/* cache_purgeID	s*/
	sa110_cache_purgeID_E,		/* cache_purgeID_E	s*/
	sa110_cache_purgeD,		/* cache_purgeD		s*/
	sa110_cache_purgeD_E,		/* cache_purgeD_E	s*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	sa110_cache_syncI,		/* cache_syncI		*/
	sa110_cache_cleanID_rng,	/* cache_cleanID_rng	*/
	sa110_cache_cleanD_rng,		/* cache_cleanD_rng	*/
	sa110_cache_purgeID_rng,	/* cache_purgeID_rng	*/
	sa110_cache_purgeD_rng,		/* cache_purgeD_rng	*/
	sa110_cache_syncI_rng,		/* cache_syncI_rng	*/

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	sa110_context_switch,		/* context_switch	*/

	sa110_setup			/* cpu setup		*/
};          
#endif	/* CPU_SA110 */

#ifdef CPU_XSCALE
struct cpu_functions xscale_cpufuncs = {
	/* CPU functions */
	
	cpufunc_id,			/* id			*/
	xscale_cpwait,			/* cpwait		*/

	/* MMU functions */

	xscale_control,			/* control		*/
	cpufunc_domains,		/* domain		*/
	xscale_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	xscale_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache functions */

	xscale_cache_flushID,		/* cache_flushID	*/
	(void *)xscale_cache_flushID,	/* cache_flushID_SE	*/
	xscale_cache_flushI,		/* cache_flushI		*/
	(void *)xscale_cache_flushI,	/* cache_flushI_SE	*/
	xscale_cache_flushD,		/* cache_flushD		*/
	xscale_cache_flushD_SE,		/* cache_flushD_SE	*/

	xscale_cache_cleanID,		/* cache_cleanID	s*/
	xscale_cache_cleanD_E,		/* cache_cleanID_E	s*/
	xscale_cache_cleanD,		/* cache_cleanD		s*/
	xscale_cache_cleanD_E,		/* cache_cleanD_E	*/

	xscale_cache_purgeID,		/* cache_purgeID	s*/
	xscale_cache_purgeID_E,		/* cache_purgeID_E	s*/
	xscale_cache_purgeD,		/* cache_purgeD		s*/
	xscale_cache_purgeD_E,		/* cache_purgeD_E	s*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	xscale_cache_syncI,		/* cache_syncI		*/
	xscale_cache_cleanID_rng,	/* cache_cleanID_rng	*/
	xscale_cache_cleanD_rng,	/* cache_cleanD_rng	*/
	xscale_cache_purgeID_rng,	/* cache_purgeID_rng	*/
	xscale_cache_purgeD_rng,	/* cache_purgeD_rng	*/
	xscale_cache_syncI_rng,		/* cache_syncI_rng	*/

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	xscale_context_switch,		/* context_switch	*/

	xscale_setup			/* cpu setup		*/
};

struct cpu_functions xscale_writethrough_cpufuncs = {
	/* CPU functions */
	
	cpufunc_id,			/* id			*/
	xscale_cpwait,			/* cpwait		*/

	/* MMU functions */

	xscale_control,			/* control		*/
	cpufunc_domains,		/* domain		*/
	xscale_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	xscale_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache functions */

	xscale_cache_flushID,		/* cache_flushID	*/
	(void *)xscale_cache_flushID,	/* cache_flushID_SE	*/
	xscale_cache_flushI,		/* cache_flushI		*/
	(void *)xscale_cache_flushI,	/* cache_flushI_SE	*/
	xscale_cache_flushD,		/* cache_flushD		*/
	xscale_cache_flushD_SE,		/* cache_flushD_SE	*/

	cpufunc_nullop,			/* cache_cleanID	s*/
	(void *)cpufunc_nullop,		/* cache_cleanID_E	s*/
	cpufunc_nullop,			/* cache_cleanD		s*/
	(void *)cpufunc_nullop,		/* cache_cleanD_E	*/

	xscale_cache_flushID,		/* cache_purgeID	s*/
	(void *)xscale_cache_flushID,	/* cache_purgeID_E	s*/
	xscale_cache_flushD,		/* cache_purgeD		s*/
	xscale_cache_flushD_SE,		/* cache_purgeD_E	s*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	xscale_cache_flushI,		/* cache_syncI		*/
	(void *)cpufunc_nullop,		/* cache_cleanID_rng	*/
	(void *)cpufunc_nullop,		/* cache_cleanD_rng	*/
	xscale_cache_flushID_rng,	/* cache_purgeID_rng	*/
	xscale_cache_flushD_rng,	/* cache_purgeD_rng	*/
	xscale_cache_flushI_rng,	/* cache_syncI_rng	*/

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	xscale_context_switch,		/* context_switch	*/

	xscale_setup			/* cpu setup		*/
};
#endif /* CPU_XSCALE */

/*
 * Global constants also used by locore.s
 */

struct cpu_functions cpufuncs;
u_int cputype;
u_int cpu_reset_needs_v4_MMU_disable;	/* flag used in locore.s */

#if defined(CPU_ARM7TDMI) || defined(CPU_ARM8) || defined(CPU_ARM9) || \
    defined(CPU_SA110) || defined(CPU_XSCALE)
static void
get_cachetype()
{
	u_int ctype, isize, dsize;
	u_int multiplier;

	__asm __volatile("mrc p15, 0, %0, c0, c0, 1"
		: "=r" (ctype));

	/*
	 * ...and thus spake the ARM ARM:
	 *
	 * If an <opcode2> value corresponding to an unimplemented or
	 * reserved ID register is encountered, the System Control
	 * processor returns the value of the main ID register.
	 */
	if (ctype == cpufunc_id())
		goto out;

	if ((ctype & CPU_CT_S) == 0)
		arm_pcache_unified = 1;

	/*
	 * If you want to know how this code works, go read the ARM ARM.
	 */

	arm_pcache_type = CPU_CT_CTYPE(ctype);

	if (arm_pcache_unified == 0) {
		isize = CPU_CT_ISIZE(ctype);
		multiplier = (isize & CPU_CT_xSIZE_M) ? 3 : 2;
		arm_picache_line_size = 1U << (CPU_CT_xSIZE_LEN(isize) + 3);
		if (CPU_CT_xSIZE_ASSOC(isize) == 0) {
			if (isize & CPU_CT_xSIZE_M)
				arm_picache_line_size = 0; /* not present */
			else
				arm_picache_ways = 1;
		} else {
			arm_picache_ways = multiplier <<
			    (CPU_CT_xSIZE_ASSOC(isize) - 1);
		}
		arm_picache_size = multiplier << (CPU_CT_xSIZE_SIZE(isize) + 8);
	}

	dsize = CPU_CT_DSIZE(ctype);
	multiplier = (dsize & CPU_CT_xSIZE_M) ? 3 : 2;
	arm_pdcache_line_size = 1U << (CPU_CT_xSIZE_LEN(dsize) + 3);
	if (CPU_CT_xSIZE_ASSOC(dsize) == 0) {
		if (dsize & CPU_CT_xSIZE_M)
			arm_pdcache_line_size = 0; /* not present */
		else
			arm_pdcache_ways = 0;
	} else {
		arm_pdcache_ways = multiplier <<
		    (CPU_CT_xSIZE_ASSOC(dsize) - 1);
	}
	arm_pdcache_size = multiplier << (CPU_CT_xSIZE_SIZE(dsize) + 8);

	arm_dcache_align = arm_pdcache_line_size;

 out:
	arm_dcache_align_mask = arm_dcache_align - 1;
}
#endif /* ARM7TDMI || ARM8 || ARM9 || SA110 || XSCALE */

/*
 * Cannot panic here as we may not have a console yet ...
 */

int
set_cpufuncs()
{
	cputype = cpufunc_id();
	cputype &= CPU_ID_CPU_MASK;


#ifdef CPU_ARM3
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    (cputype & 0x00000f00) == 0x00000300) {
		cpufuncs = arm3_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 0;
		/* XXX Cache info? */
		arm_dcache_align_mask = -1;
		return 0;
	}
#endif	/* CPU_ARM3 */
#ifdef CPU_ARM6
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    (cputype & 0x00000f00) == 0x00000600) {
		cpufuncs = arm6_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 0;
		/* XXX Cache info? */
		arm_dcache_align_mask = -1;
		return 0;
	}
#endif	/* CPU_ARM6 */
#ifdef CPU_ARM7
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    CPU_ID_IS7(cputype) &&
	    (cputype & CPU_ID_7ARCH_MASK) == CPU_ID_7ARCH_V3) {
		cpufuncs = arm7_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 0;
		/* XXX Cache info? */
		arm_dcache_align_mask = -1;
		return 0;
	}
#endif	/* CPU_ARM7 */
#ifdef CPU_ARM7TDMI
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    CPU_ID_IS7(cputype) &&
	    (cputype & CPU_ID_7ARCH_MASK) == CPU_ID_7ARCH_V4T) {
		cpufuncs = arm7tdmi_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 0;
		get_cachetype();
		return 0;
	}
#endif	
#ifdef CPU_ARM8
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    (cputype & 0x0000f000) == 0x00008000) {
		cpufuncs = arm8_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 0;	/* XXX correct? */
		get_cachetype();
		return 0;
	}
#endif	/* CPU_ARM8 */
#ifdef CPU_ARM9
	if (cputype == CPU_ID_ARM920T) {
		pte_cache_mode = PT_C;	/* Select write-through cacheing. */
		cpufuncs = arm9_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* V4 or higher */
		get_cachetype();
		return 0;
	}
#endif /* CPU_ARM9 */
#ifdef CPU_SA110
	if (cputype == CPU_ID_SA110 || cputype == CPU_ID_SA1100 ||
	    cputype == CPU_ID_SA1110) {
		cpufuncs = sa110_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* SA needs it */
		get_cachetype();
		return 0;
	}
#endif	/* CPU_SA110 */
#ifdef CPU_XSCALE
	if (cputype == CPU_ID_I80200) {
		/*
		 * Reset the Interrupt Controller Unit to a pristine
		 * state:
		 *	- all interrupt sources disabled
		 *	- PMU/BCU sterred to IRQ
		 */
		__asm __volatile("mcr p13, 0, %0, c0, c0, 0"
			:
			: "r" (0));
		__asm __volatile("mcr p13, 0, %0, c2, c0, 0"
			:
			: "r" (0));

		/*
		 * Reset the Performance Monitoring Unit to a
		 * pristine state:
		 *	- CCNT, PMN0, PMN1 reset to 0
		 *	- overflow indications cleared
		 *	- all counters disabled
		 */
		__asm __volatile("mcr p14, 0, %0, c0, c0, 0"
			:
			: "r" (PMNC_P|PMNC_C|PMNC_PMN0_IF|PMNC_PMN1_IF|
			       PMNC_CC_IF));

		/*
		 * XXX Disable ECC in the Bus Controller Unit; we
		 * don't really support it, yet.  Clear any pending
		 * error indications.
		 */
		__asm __volatile("mcr p13, 0, %0, c0, c1, 0"
			:
			: "r" (BCUCTL_E0|BCUCTL_E1|BCUCTL_EV));

		pte_cache_mode = PT_C;	/* Select write-through cacheing. */
		cpufuncs = xscale_writethrough_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype();
		return 0;
	}
#endif /* CPU_XSCALE */
	/*
	 * Bzzzz. And the answer was ...
	 */
/*	panic("No support for this CPU type (%08x) in kernel", cputype);*/
	return(ARCHITECTURE_NOT_PRESENT);
}

/*
 * Fixup routines for data and prefetch aborts.
 *
 * Several compile time symbols are used
 *
 * DEBUG_FAULT_CORRECTION - Print debugging information during the
 * correction of registers after a fault.
 * ARM6_LATE_ABORT - ARM6 supports both early and late aborts
 * when defined should use late aborts
 */

#if defined(DEBUG_FAULT_CORRECTION) && !defined(PMAP_DEBUG)
#error PMAP_DEBUG must be defined to use DEBUG_FAULT_CORRECTION
#endif

/*
 * Null abort fixup routine.
 * For use when no fixup is required.
 */
int
cpufunc_null_fixup(arg)
	void *arg;
{
	return(ABORT_FIXUP_OK);
}

#if defined(CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_ARM7TDMI)
#ifdef DEBUG_FAULT_CORRECTION
extern int pmap_debug_level;
#endif
#endif

#if defined(CPU_ARM2) || defined(CPU_ARM250) || defined(CPU_ARM3) || \
    defined(CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_ARM7TDMI)
/*
 * "Early" data abort fixup.
 *
 * For ARM2, ARM2as, ARM3 and ARM6 (in early-abort mode).  Also used
 * indirectly by ARM6 (in late-abort mode) and ARM7[TDMI].
 *
 * In early aborts, we may have to fix up LDM, STM, LDC and STC.
 */
int
early_abort_fixup(arg)
	void *arg;
{
	trapframe_t *frame = arg;
	u_int fault_pc;
	u_int fault_instruction;
	int saved_lr = 0;

	if ((frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE) {

		/* Ok an abort in SVC mode */

		/*
		 * Copy the SVC r14 into the usr r14 - The usr r14 is garbage
		 * as the fault happened in svc mode but we need it in the
		 * usr slot so we can treat the registers as an array of ints
		 * during fixing.
		 * NOTE: This PC is in the position but writeback is not
		 * allowed on r15.
		 * Doing it like this is more efficient than trapping this
		 * case in all possible locations in the following fixup code.
		 */

		saved_lr = frame->tf_usr_lr;
		frame->tf_usr_lr = frame->tf_svc_lr;

		/*
		 * Note the trapframe does not have the SVC r13 so a fault
		 * from an instruction with writeback to r13 in SVC mode is
		 * not allowed. This should not happen as the kstack is
		 * always valid.
		 */
	}

	/* Get fault address and status from the CPU */

	fault_pc = frame->tf_pc;
	fault_instruction = *((volatile unsigned int *)fault_pc);

	/* Decode the fault instruction and fix the registers as needed */

	if ((fault_instruction & 0x0e000000) == 0x08000000) {
		int base;
		int loop;
		int count;
		int *registers = &frame->tf_r0;
        
#ifdef DEBUG_FAULT_CORRECTION
		if (pmap_debug_level >= 0) {
			printf("LDM/STM\n");
			disassemble(fault_pc);
		}
#endif	/* DEBUG_FAULT_CORRECTION */
		if (fault_instruction & (1 << 21)) {
#ifdef DEBUG_FAULT_CORRECTION
			if (pmap_debug_level >= 0)
				printf("This instruction must be corrected\n");
#endif	/* DEBUG_FAULT_CORRECTION */
			base = (fault_instruction >> 16) & 0x0f;
			if (base == 15)
				return ABORT_FIXUP_FAILED;
			/* Count registers transferred */
			count = 0;
			for (loop = 0; loop < 16; ++loop) {
				if (fault_instruction & (1<<loop))
					++count;
			}
#ifdef DEBUG_FAULT_CORRECTION
			if (pmap_debug_level >= 0) {
				printf("%d registers used\n", count);
				printf("Corrected r%d by %d bytes ", base, count * 4);
			}
#endif	/* DEBUG_FAULT_CORRECTION */
			if (fault_instruction & (1 << 23)) {
#ifdef DEBUG_FAULT_CORRECTION
				if (pmap_debug_level >= 0)
					printf("down\n");
#endif	/* DEBUG_FAULT_CORRECTION */
				registers[base] -= count * 4;
			} else {
#ifdef DEBUG_FAULT_CORRECTION
				if (pmap_debug_level >= 0)
					printf("up\n");
#endif	/* DEBUG_FAULT_CORRECTION */
				registers[base] += count * 4;
			}
		}
	} else if ((fault_instruction & 0x0e000000) == 0x0c000000) {
		int base;
		int offset;
		int *registers = &frame->tf_r0;
	
/* REGISTER CORRECTION IS REQUIRED FOR THESE INSTRUCTIONS */

#ifdef DEBUG_FAULT_CORRECTION
		if (pmap_debug_level >= 0)
			disassemble(fault_pc);
#endif	/* DEBUG_FAULT_CORRECTION */

/* Only need to fix registers if write back is turned on */

		if ((fault_instruction & (1 << 21)) != 0) {
			base = (fault_instruction >> 16) & 0x0f;
			if (base == 13 && (frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE)
				return ABORT_FIXUP_FAILED;
			if (base == 15)
				return ABORT_FIXUP_FAILED;

			offset = (fault_instruction & 0xff) << 2;
#ifdef DEBUG_FAULT_CORRECTION
			if (pmap_debug_level >= 0)
				printf("r%d=%08x\n", base, registers[base]);
#endif	/* DEBUG_FAULT_CORRECTION */
			if ((fault_instruction & (1 << 23)) != 0)
				offset = -offset;
			registers[base] += offset;
#ifdef DEBUG_FAULT_CORRECTION
			if (pmap_debug_level >= 0)
				printf("r%d=%08x\n", base, registers[base]);
#endif	/* DEBUG_FAULT_CORRECTION */
		}
	} else if ((fault_instruction & 0x0e000000) == 0x0c000000)
		return ABORT_FIXUP_FAILED;

	if ((frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE) {

		/* Ok an abort in SVC mode */

		/*
		 * Copy the SVC r14 into the usr r14 - The usr r14 is garbage
		 * as the fault happened in svc mode but we need it in the
		 * usr slot so we can treat the registers as an array of ints
		 * during fixing.
		 * NOTE: This PC is in the position but writeback is not
		 * allowed on r15.
		 * Doing it like this is more efficient than trapping this
		 * case in all possible locations in the prior fixup code.
		 */

		frame->tf_svc_lr = frame->tf_usr_lr;
		frame->tf_usr_lr = saved_lr;

		/*
		 * Note the trapframe does not have the SVC r13 so a fault
		 * from an instruction with writeback to r13 in SVC mode is
		 * not allowed. This should not happen as the kstack is
		 * always valid.
		 */
	}

	return(ABORT_FIXUP_OK);
}
#endif	/* CPU_ARM2/250/3/6/7 */

#if (defined(CPU_ARM6) && defined(ARM6_LATE_ABORT)) || defined(CPU_ARM7) || \
	defined(CPU_ARM7TDMI)
/*
 * "Late" (base updated) data abort fixup
 *
 * For ARM6 (in late-abort mode) and ARM7.
 *
 * In this model, all data-transfer instructions need fixing up.  We defer
 * LDM, STM, LDC and STC fixup to the early-abort handler.
 */
int
late_abort_fixup(arg)
	void *arg;
{
	trapframe_t *frame = arg;
	u_int fault_pc;
	u_int fault_instruction;
	int saved_lr = 0;

	if ((frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE) {

		/* Ok an abort in SVC mode */

		/*
		 * Copy the SVC r14 into the usr r14 - The usr r14 is garbage
		 * as the fault happened in svc mode but we need it in the
		 * usr slot so we can treat the registers as an array of ints
		 * during fixing.
		 * NOTE: This PC is in the position but writeback is not
		 * allowed on r15.
		 * Doing it like this is more efficient than trapping this
		 * case in all possible locations in the following fixup code.
		 */

		saved_lr = frame->tf_usr_lr;
		frame->tf_usr_lr = frame->tf_svc_lr;

		/*
		 * Note the trapframe does not have the SVC r13 so a fault
		 * from an instruction with writeback to r13 in SVC mode is
		 * not allowed. This should not happen as the kstack is
		 * always valid.
		 */
	}

	/* Get fault address and status from the CPU */

	fault_pc = frame->tf_pc;
	fault_instruction = *((volatile unsigned int *)fault_pc);

	/* Decode the fault instruction and fix the registers as needed */

	/* Was is a swap instruction ? */

	if ((fault_instruction & 0x0fb00ff0) == 0x01000090) {
#ifdef DEBUG_FAULT_CORRECTION
		if (pmap_debug_level >= 0)
			disassemble(fault_pc);
#endif	/* DEBUG_FAULT_CORRECTION */
	} else if ((fault_instruction & 0x0c000000) == 0x04000000) {

		/* Was is a ldr/str instruction */
		/* This is for late abort only */

		int base;
		int offset;
		int *registers = &frame->tf_r0;

#ifdef DEBUG_FAULT_CORRECTION
		if (pmap_debug_level >= 0)
			disassemble(fault_pc);
#endif	/* DEBUG_FAULT_CORRECTION */
		
		/* This is for late abort only */

		if ((fault_instruction & (1 << 24)) == 0
		    || (fault_instruction & (1 << 21)) != 0) {
			base = (fault_instruction >> 16) & 0x0f;
			if (base == 13 && (frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE)
				return ABORT_FIXUP_FAILED;
			if (base == 15)
				return ABORT_FIXUP_FAILED;
#ifdef DEBUG_FAULT_CORRECTION
			if (pmap_debug_level >=0)
				printf("late abt fix: r%d=%08x ", base, registers[base]);
#endif	/* DEBUG_FAULT_CORRECTION */
			if ((fault_instruction & (1 << 25)) == 0) {
				/* Immediate offset - easy */                  
				offset = fault_instruction & 0xfff;
				if ((fault_instruction & (1 << 23)))
					offset = -offset;
				registers[base] += offset;
#ifdef DEBUG_FAULT_CORRECTION
				if (pmap_debug_level >=0)
					printf("imm=%08x ", offset);
#endif	/* DEBUG_FAULT_CORRECTION */
			} else {
				int shift;

				offset = fault_instruction & 0x0f;
				if (offset == base)
					return ABORT_FIXUP_FAILED;
                
/* Register offset - hard we have to cope with shifts ! */
				offset = registers[offset];

				if ((fault_instruction & (1 << 4)) == 0)
					shift = (fault_instruction >> 7) & 0x1f;
				else {
					if ((fault_instruction & (1 << 7)) != 0)
						return ABORT_FIXUP_FAILED;
					shift = ((fault_instruction >> 8) & 0xf);
					if (base == shift)
						return ABORT_FIXUP_FAILED;
#ifdef DEBUG_FAULT_CORRECTION
					if (pmap_debug_level >=0)
						printf("shift reg=%d ", shift);
#endif	/* DEBUG_FAULT_CORRECTION */
					shift = registers[shift];
				}
#ifdef DEBUG_FAULT_CORRECTION
				if (pmap_debug_level >=0)
					printf("shift=%08x ", shift);
#endif	/* DEBUG_FAULT_CORRECTION */
				switch (((fault_instruction >> 5) & 0x3)) {
				case 0 : /* Logical left */
					offset = (int)(((u_int)offset) << shift);
					break;
				case 1 : /* Logical Right */
					if (shift == 0) shift = 32;
					offset = (int)(((u_int)offset) >> shift);
					break;
				case 2 : /* Arithmetic Right */
					if (shift == 0) shift = 32;
					offset = (int)(((int)offset) >> shift);
					break;
				case 3 : /* Rotate right */
					return ABORT_FIXUP_FAILED;
				}

#ifdef DEBUG_FAULT_CORRECTION
				if (pmap_debug_level >=0)
					printf("abt: fixed LDR/STR with register offset\n");
#endif	/* DEBUG_FAULT_CORRECTION */               
				if ((fault_instruction & (1 << 23)))
					offset = -offset;
#ifdef DEBUG_FAULT_CORRECTION
				if (pmap_debug_level >=0)
					printf("offset=%08x ", offset);
#endif	/* DEBUG_FAULT_CORRECTION */
				registers[base] += offset;
			}
#ifdef DEBUG_FAULT_CORRECTION
			if (pmap_debug_level >=0)
				printf("r%d=%08x\n", base, registers[base]);
#endif	/* DEBUG_FAULT_CORRECTION */
		}
	}

	if ((frame->tf_spsr & PSR_MODE) == PSR_SVC32_MODE) {

		/* Ok an abort in SVC mode */

		/*
		 * Copy the SVC r14 into the usr r14 - The usr r14 is garbage
		 * as the fault happened in svc mode but we need it in the
		 * usr slot so we can treat the registers as an array of ints
		 * during fixing.
		 * NOTE: This PC is in the position but writeback is not
		 * allowed on r15.
		 * Doing it like this is more efficient than trapping this
		 * case in all possible locations in the prior fixup code.
		 */

		frame->tf_svc_lr = frame->tf_usr_lr;
		frame->tf_usr_lr = saved_lr;

		/*
		 * Note the trapframe does not have the SVC r13 so a fault
		 * from an instruction with writeback to r13 in SVC mode is
		 * not allowed. This should not happen as the kstack is
		 * always valid.
		 */
	}

	/*
	 * Now let the early-abort fixup routine have a go, in case it
	 * was an LDM, STM, LDC or STC that faulted.
	 */

	return early_abort_fixup(arg);
}
#endif	/* CPU_ARM6(LATE)/7/7TDMI */

/*
 * CPU Setup code
 */

#if defined(CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_ARM7TDMI) || \
	defined(CPU_ARM8) || defined (CPU_ARM9) || defined(CPU_SA110) || \
	defined(CPU_XSCALE)
int cpuctrl;

#define IGN	0
#define OR	1
#define BIC	2

struct cpu_option {
	char	*co_name;
	int	co_falseop;
	int	co_trueop;
	int	co_value;
};

static u_int
parse_cpu_options(args, optlist, cpuctrl)
	char *args;
	struct cpu_option *optlist;    
	u_int cpuctrl; 
{
	int integer;

	while (optlist->co_name) {
		if (get_bootconf_option(args, optlist->co_name,
		    BOOTOPT_TYPE_BOOLEAN, &integer)) {
			if (integer) {
				if (optlist->co_trueop == OR)
					cpuctrl |= optlist->co_value;
				else if (optlist->co_trueop == BIC)
					cpuctrl &= ~optlist->co_value;
			} else {
				if (optlist->co_falseop == OR)
					cpuctrl |= optlist->co_value;
				else if (optlist->co_falseop == BIC)
					cpuctrl &= ~optlist->co_value;
			}
		}
		++optlist;
	}
	return(cpuctrl);
}
#endif /* CPU_ARM6 || CPU_ARM7 || CPU_ARM7TDMI || CPU_ARM8 || CPU_SA110 */

#if defined (CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_ARM7TDMI) \
	|| defined(CPU_ARM8)
struct cpu_option arm678_options[] = {
#ifdef COMPAT_12
	{ "nocache",		IGN, BIC, CPU_CONTROL_IDC_ENABLE },
	{ "nowritebuf",		IGN, BIC, CPU_CONTROL_WBUF_ENABLE },
#endif	/* COMPAT_12 */
	{ "cpu.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "cpu.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

#endif	/* CPU_ARM6 || CPU_ARM7 || CPU_ARM7TDMI || CPU_ARM8 */

#ifdef CPU_ARM6
struct cpu_option arm6_options[] = {
	{ "arm6.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "arm6.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "arm6.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "arm6.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
arm6_setup(args)
	char *args;
{
	int cpuctrlmask;

	/* Set up default control registers bits */
	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_BEND_ENABLE
		 | CPU_CONTROL_AFLT_ENABLE;

#ifdef ARM6_LATE_ABORT
	cpuctrl |= CPU_CONTROL_LABT_ENABLE;
#endif	/* ARM6_LATE_ABORT */

	cpuctrl = parse_cpu_options(args, arm678_options, cpuctrl);
	cpuctrl = parse_cpu_options(args, arm6_options, cpuctrl);

	/* Clear out the cache */
	cpu_cache_purgeID();

	/* Set the control register */    
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_ARM6 */

#ifdef CPU_ARM7
struct cpu_option arm7_options[] = {
	{ "arm7.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "arm7.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "arm7.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "arm7.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
#ifdef COMPAT_12
	{ "fpaclk2",		BIC, OR,  CPU_CONTROL_CPCLK },
#endif	/* COMPAT_12 */
	{ "arm700.fpaclk",	BIC, OR,  CPU_CONTROL_CPCLK },
	{ NULL,			IGN, IGN, 0 }
};

void
arm7_setup(args)
	char *args;
{
	int cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_LABT_ENABLE
		 | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_BEND_ENABLE
		 | CPU_CONTROL_AFLT_ENABLE;

	cpuctrl = parse_cpu_options(args, arm678_options, cpuctrl);
	cpuctrl = parse_cpu_options(args, arm7_options, cpuctrl);

	/* Clear out the cache */
	cpu_cache_purgeID();

	/* Set the control register */    
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_ARM7 */

#ifdef CPU_ARM7TDMI
struct cpu_option arm7tdmi_options[] = {
	{ "arm7.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "arm7.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "arm7.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "arm7.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
#ifdef COMPAT_12
	{ "fpaclk2",		BIC, OR,  CPU_CONTROL_CPCLK },
#endif	/* COMPAT_12 */
	{ "arm700.fpaclk",	BIC, OR,  CPU_CONTROL_CPCLK },
	{ NULL,			IGN, IGN, 0 }
};

void
arm7tdmi_setup(args)
	char *args;
{
	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE;

	cpuctrl = parse_cpu_options(args, arm678_options, cpuctrl);
	cpuctrl = parse_cpu_options(args, arm7tdmi_options, cpuctrl);

	/* Clear out the cache */
	cpu_cache_purgeID();

	/* Set the control register */    
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_ARM7TDMI */

#ifdef CPU_ARM8
struct cpu_option arm8_options[] = {
	{ "arm8.cache",		BIC, OR,  CPU_CONTROL_IDC_ENABLE },
	{ "arm8.nocache",	OR,  BIC, CPU_CONTROL_IDC_ENABLE },
	{ "arm8.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "arm8.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
#ifdef COMPAT_12
	{ "branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
#endif	/* COMPAT_12 */
	{ "cpu.branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "arm8.branchpredict",	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
arm8_setup(args)
	char *args;
{
	int integer;
	int cpuctrlmask;
	int clocktest;
	int setclock = 0;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_BPRD_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE;

	cpuctrl = parse_cpu_options(args, arm678_options, cpuctrl);
	cpuctrl = parse_cpu_options(args, arm8_options, cpuctrl);

	/* Get clock configuration */
	clocktest = arm8_clock_config(0, 0) & 0x0f;

	/* Special ARM8 clock and test configuration */
	if (get_bootconf_option(args, "arm8.clock.reset", BOOTOPT_TYPE_BOOLEAN, &integer)) {
		clocktest = 0;
		setclock = 1;
	}
	if (get_bootconf_option(args, "arm8.clock.dynamic", BOOTOPT_TYPE_BOOLEAN, &integer)) {
		if (integer)
			clocktest |= 0x01;
		else
			clocktest &= ~(0x01);
		setclock = 1;
	}
	if (get_bootconf_option(args, "arm8.clock.sync", BOOTOPT_TYPE_BOOLEAN, &integer)) {
		if (integer)
			clocktest |= 0x02;
		else
			clocktest &= ~(0x02);
		setclock = 1;
	}
	if (get_bootconf_option(args, "arm8.clock.fast", BOOTOPT_TYPE_BININT, &integer)) {
		clocktest = (clocktest & ~0xc0) | (integer & 3) << 2;
		setclock = 1;
	}
	if (get_bootconf_option(args, "arm8.test", BOOTOPT_TYPE_BININT, &integer)) {
		clocktest |= (integer & 7) << 5;
		setclock = 1;
	}
	
	/* Clear out the cache */
	cpu_cache_purgeID();

	/* Set the control register */    
	cpu_control(0xffffffff, cpuctrl);

	/* Set the clock/test register */    
	if (setclock)
		arm8_clock_config(0x7f, clocktest);
}
#endif	/* CPU_ARM8 */

#ifdef CPU_ARM9
struct cpu_option arm9_options[] = {
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "arm9.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "arm9.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "arm9.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ "arm9.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
arm9_setup(args)
	char *args;
{
	int cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
	    | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK;

	cpuctrl = parse_cpu_options(args, arm9_options, cpuctrl);

	/* Clear out the cache */
	cpu_cache_purgeID();

	/* Set the control register */    
	cpu_control(0xffffffff, cpuctrl);

}
#endif	/* CPU_ARM9 */

#ifdef CPU_SA110
struct cpu_option sa110_options[] = {
#ifdef COMPAT_12
	{ "nocache",		IGN, BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "nowritebuf",		IGN, BIC, CPU_CONTROL_WBUF_ENABLE },
#endif	/* COMPAT_12 */
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "sa110.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "sa110.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "sa110.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ "cpu.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ "cpu.nowritebuf",	OR,  BIC, CPU_CONTROL_WBUF_ENABLE },
	{ "sa110.writebuf",	BIC, OR,  CPU_CONTROL_WBUF_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
sa110_setup(args)
	char *args;
{
	int cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK;

	cpuctrl = parse_cpu_options(args, sa110_options, cpuctrl);

	/* Clear out the cache */
	cpu_cache_purgeID();

	/* Set the control register */    
/*	cpu_control(cpuctrlmask, cpuctrl);*/
	cpu_control(0xffffffff, cpuctrl);

	/* 
	 * enable clockswitching, note that this doesn't read or write to r0,
	 * r0 is just to make it valid asm
	 */
	__asm ("mcr 15, 0, r0, c15, c1, 2");
}
#endif	/* CPU_SA110 */

#ifdef CPU_XSCALE
struct cpu_option xscale_options[] = {
#ifdef COMPAT_12
	{ "branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "nocache",		IGN, BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
#endif	/* COMPAT_12 */
	{ "cpu.branchpredict", 	BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "cpu.cache",		BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "cpu.nocache",	OR,  BIC, (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "xscale.branchpredict", BIC, OR,  CPU_CONTROL_BPRD_ENABLE },
	{ "xscale.cache",	BIC, OR,  (CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE) },
	{ "xscale.icache",	BIC, OR,  CPU_CONTROL_IC_ENABLE },
	{ "xscale.dcache",	BIC, OR,  CPU_CONTROL_DC_ENABLE },
	{ NULL,			IGN, IGN, 0 }
};

void
xscale_setup(args)
	char *args;
{
	int cpuctrlmask;

	/*
	 * The XScale Write Buffer is always enabled.  Our option
	 * is to enable/disable coalescing.  Note that bits 6:3
	 * must always be enabled.
	 */

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK;

	cpuctrl = parse_cpu_options(args, xscale_options, cpuctrl);

	/* Clear out the cache */
	cpu_cache_purgeID();

	/*
	 * Set the control register.  Note that bits 6:3 must always
	 * be set to 1.
	 */
/*	cpu_control(cpuctrlmask, cpuctrl);*/
	cpu_control(0xffffffff, cpuctrl);

#if 0
	/*
	 * XXX FIXME
	 * Disable write buffer coalescing, PT ECC, and set
	 * the mini-cache to write-back/read-allocate.
	 */
	__asm ("mcr p15, 0, %0, c1, c0, 1" :: "r" (0));
#endif
}
#endif	/* CPU_XSCALE */

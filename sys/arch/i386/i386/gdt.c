#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/gdt.h>

#define	MINGDTSIZ	512
#define	MAXGDTSIZ	8192

union descriptor *dynamic_gdt = gdt;
int gdt_size = NGDT;		/* total number of GDT entries */
int gdt_count = NGDT;		/* number of GDT entries in use */
int gdt_next = NGDT;		/* next available slot for sweeping */
int gdt_free = GNULL_SEL;	/* next free slot; terminated with GNULL_SEL */

int gdt_flags;
#define	GDT_LOCKED	0x1
#define	GDT_WANTED	0x2

/*
 * Lock and unlock the GDT, to avoid races in case gdt_{ge,pu}t_slot() sleep
 * waiting for memory.
 *
 * Note that the locking done here is not sufficient for multiprocessor
 * systems.  A freshly allocated slot will still be of type SDT_SYSNULL for
 * some time after the GDT is unlocked, so gdt_compact() could attempt to
 * reclaim it.
 */
static inline void
gdt_lock()
{

	while ((gdt_flags & GDT_LOCKED) != 0) {
		gdt_flags |= GDT_WANTED;
		tsleep(&gdt_flags, PZERO, "gdtlck", 0);
	}
	gdt_flags |= GDT_LOCKED;
}

static inline void
gdt_unlock()
{

	gdt_flags &= ~GDT_LOCKED;
	if ((gdt_flags & GDT_WANTED) != 0) {
		gdt_flags &= ~GDT_WANTED;
		wakeup(&gdt_flags);
	}
}

/*
 * Compact the GDT as follows:
 * 0) We partition the GDT into two areas, one of the slots before gdt_count,
 *    and one of the slots after.  After compaction, the former part should be
 *    completely filled, and the latter part should be completely empty.
 * 1) Step through the process list, looking for TSS and LDT descriptors in
 *    the second section, and swap them with empty slots in the first section.
 * 2) Arrange for new allocations to sweep through the empty section.  Since
 *    we're sweeping through all of the empty entries, and we'll create a free
 *    list as things are deallocated, we do not need to create a new free list
 *    here.
 */
void
gdt_compact()
{
	struct proc *p;
	struct pcb *pcb;
	int slot = NGDT, oslot;

	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		pcb = &p->p_addr->u_pcb;
		oslot = IDXSEL(pcb->pcb_tss_sel);
		if (oslot >= gdt_count) {
			while (dynamic_gdt[slot].sd.sd_type != SDT_SYSNULL) {
				if (++slot >= gdt_count)
					panic("gdt_compact botch 1");
			}
			dynamic_gdt[slot] = dynamic_gdt[oslot];
			dynamic_gdt[oslot].gd.gd_type = SDT_SYSNULL;
			pcb->pcb_tss_sel = GSEL(slot, SEL_KPL);
		}
		oslot = IDXSEL(pcb->pcb_ldt_sel);
		if (oslot >= gdt_count) {
			while (dynamic_gdt[slot].sd.sd_type != SDT_SYSNULL) {
				if (++slot >= gdt_count)
					panic("gdt_compact botch 2");
			}
			dynamic_gdt[slot] = dynamic_gdt[oslot];
			dynamic_gdt[oslot].gd.gd_type = SDT_SYSNULL;
			pcb->pcb_ldt_sel = GSEL(slot, SEL_KPL);
		}
	}
	for (; slot < gdt_count; slot++)
		if (dynamic_gdt[slot].gd.gd_type == SDT_SYSNULL)
			panic("gdt_compact botch 3");
	for (slot = gdt_count; slot < gdt_size; slot++)
		if (dynamic_gdt[slot].gd.gd_type != SDT_SYSNULL)
			panic("gdt_compact botch 4");
	gdt_next = gdt_count;
	gdt_free = GNULL_SEL;
}

/*
 * Grow or shrink the GDT.
 */
void
gdt_init()
{
	size_t max_len, min_len;
	struct region_descriptor region;

	max_len = MAXGDTSIZ * sizeof(union descriptor);
	min_len = MINGDTSIZ * sizeof(union descriptor);
	gdt_size = MINGDTSIZ;

	dynamic_gdt = (union descriptor *)kmem_alloc_pageable(kernel_map,
	    max_len);
	vm_map_pageable(kernel_map, (vm_offset_t)dynamic_gdt,
	    (vm_offset_t)dynamic_gdt + min_len, FALSE);
	bcopy(gdt, dynamic_gdt, NGDT * sizeof(union descriptor));

	setregion(&region, dynamic_gdt, max_len - 1);
	lgdt(&region);
}

void
gdt_grow()
{
	size_t old_len, new_len;

	old_len = gdt_size * sizeof(union descriptor);
	gdt_size <<= 1;
	new_len = old_len << 1;

	vm_map_pageable(kernel_map, (vm_offset_t)dynamic_gdt + old_len,
	    (vm_offset_t)dynamic_gdt + new_len, FALSE);
}

void
gdt_shrink()
{
	size_t old_len, new_len;

	old_len = gdt_size * sizeof(union descriptor);
	gdt_size >>= 1;
	new_len = old_len >> 1;

	vm_map_pageable(kernel_map, (vm_offset_t)dynamic_gdt + new_len,
	    (vm_offset_t)dynamic_gdt + old_len, TRUE);
}

/*
 * Allocate a GDT slot as follows:
 * 1) If there are entries on the free list, use those.
 * 2) If there are fewer than gdt_size entries in use, there are free slots
 *    near the end that we can sweep through.
 * 3) As a last resort, we increase the size of the GDT, and sweep through
 *    the new slots.
 */
int
gdt_get_slot()
{
	int slot;

	gdt_lock();

	if (gdt_free != GNULL_SEL) {
		slot = gdt_free;
		gdt_free = dynamic_gdt[slot].gd.gd_selector;
	} else {
		if (gdt_next != gdt_count)
			panic("gdt_get_slot botch 1");
		if (gdt_next >= gdt_size) {
			if (gdt_size >= MAXGDTSIZ)
				panic("gdt_get_slot botch 2");
			if (dynamic_gdt == gdt)
				gdt_init();
			else
				gdt_grow();
		}
		slot = gdt_next++;
	}

	gdt_count++;
	gdt_unlock();
	return (slot);
}

/*
 * Deallocate a GDT slot, putting it on the free list.
 */
void
gdt_put_slot(slot)
	int slot;
{

	gdt_lock();
	gdt_count--;

	dynamic_gdt[slot].gd.gd_type = SDT_SYSNULL;
	/* 
	 * shrink the GDT if we're using less than 1/4 of it.
	 * Shrinking at that point means we'll still have room for
	 * almost 2x as many processes as are now running without
	 * having to grow the GDT.
	 */
	if (gdt_size > MINGDTSIZ && gdt_count <= gdt_size / 4) {
		gdt_compact();
		gdt_shrink();
	} else {
		dynamic_gdt[slot].gd.gd_selector = gdt_free;
		gdt_free = slot;
	}

	gdt_unlock();
}

void
tss_alloc(pcb)
	struct pcb *pcb;
{
	int slot;

	slot = gdt_get_slot();
	setsegment(&dynamic_gdt[slot].sd, &pcb->pcb_tss, sizeof(struct pcb) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	pcb->pcb_tss_sel = GSEL(slot, SEL_KPL);
}

void
tss_free(pcb)
	struct pcb *pcb;
{

	gdt_put_slot(IDXSEL(pcb->pcb_tss_sel));
}

void
ldt_alloc(pcb, ldt, len)
	struct pcb *pcb;
	union descriptor *ldt;
	size_t len;
{
	int slot;

	slot = gdt_get_slot();
	setsegment(&dynamic_gdt[slot].sd, ldt, len - 1, SDT_SYSLDT, SEL_KPL, 0,
	    0);
	pcb->pcb_ldt_sel = GSEL(slot, SEL_KPL);
}

void
ldt_free(pcb)
	struct pcb *pcb;
{

	gdt_put_slot(IDXSEL(pcb->pcb_ldt_sel));
}

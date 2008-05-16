/* $NetBSD: udf_allocation.c,v 1.1.2.2 2008/05/16 02:25:21 yamt Exp $ */

/*
 * Copyright (c) 2006, 2008 Reinoud Zandijk
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
 * 
 */

#include <sys/cdefs.h>
#ifndef lint
__KERNEL_RCSID(0, "$NetBSD: udf_allocation.c,v 1.1.2.2 2008/05/16 02:25:21 yamt Exp $");
#endif /* not lint */


#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#include "opt_compat_netbsd.h"
#endif

/* TODO strip */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <miscfs/genfs/genfs_node.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/kthread.h>
#include <dev/clock_subr.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>

#if defined(_KERNEL_OPT)
#include "opt_udf.h"
#endif

#include "udf.h"
#include "udf_subr.h"
#include "udf_bswap.h"


#define VTOI(vnode) ((struct udf_node *) vnode->v_data)

static void udf_record_allocation_in_node(struct udf_mount *ump,
	struct buf *buf, uint16_t vpart_num, uint64_t *mapping,
	struct long_ad *node_ad_cpy);

/*
 * IDEA/BUSY: Each udf_node gets its own extentwalker state for all operations;
 * this will hopefully/likely reduce O(nlog(n)) to O(1) for most functionality
 * since actions are most likely sequencial and thus seeking doesn't need
 * searching for the same or adjacent position again.
 */

/* --------------------------------------------------------------------- */
//#ifdef DEBUG
#if 1
#if 1
static void
udf_node_dump(struct udf_node *udf_node) {
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct icb_tag *icbtag;
	struct short_ad *short_ad;
	struct long_ad  *long_ad;
	uint64_t inflen;
	uint32_t icbflags, addr_type, max_l_ad;
	uint32_t len, lb_num;
	uint8_t  *data_pos;
	int part_num;
	int adlen, ad_off, dscr_size, l_ea, l_ad, lb_size, flags;

	if ((udf_verbose & UDF_DEBUG_ADWLK) == 0)
		return;

	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag = &fe->icbtag;
		inflen = udf_rw64(fe->inf_len);
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
		data_pos = (uint8_t *) fe + dscr_size + l_ea;
	} else {
		icbtag = &efe->icbtag;
		inflen = udf_rw64(efe->inf_len);
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
		data_pos = (uint8_t *) efe + dscr_size + l_ea;
	}
	max_l_ad = lb_size - dscr_size - l_ea;

	icbflags   = udf_rw16(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	printf("udf_node_dump:\n");
	printf("\tudf_node %p\n", udf_node);

	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		printf("\t\tIntern alloc, len = %"PRIu64"\n", inflen);
		return;
	}

	printf("\t\tInflen  = %"PRIu64"\n", inflen);
	printf("\t\tl_ad    = %d\n", l_ad);

	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		adlen = sizeof(struct short_ad);
	} else {
		adlen = sizeof(struct long_ad);
	}

	printf("\t\t");
	for (ad_off = 0; ad_off < max_l_ad-adlen; ad_off += adlen) {
		if (addr_type == UDF_ICB_SHORT_ALLOC) {
			short_ad = (struct short_ad *) (data_pos + ad_off);
			len      = udf_rw32(short_ad->len);
			lb_num   = udf_rw32(short_ad->lb_num); 
			part_num = -1;
			flags = UDF_EXT_FLAGS(len);
			len   = UDF_EXT_LEN(len);
		} else {
			long_ad  = (struct long_ad *) (data_pos + ad_off);
			len      = udf_rw32(long_ad->len);
			lb_num   = udf_rw32(long_ad->loc.lb_num);
			part_num = udf_rw16(long_ad->loc.part_num);
			flags = UDF_EXT_FLAGS(len);
			len   = UDF_EXT_LEN(len);
		}
		printf("[");
		if (part_num >= 0)
			printf("part %d, ", part_num);
		printf("lb_num %d, len %d", lb_num, len);
		if (flags)
			printf(", flags %d", flags);
		printf("] ");
		if (ad_off + adlen == l_ad)
			printf("\n\t\tl_ad END\n\t\t");
	}
	printf("\n");
}
#else
#define udf_node_dump(a)
#endif

static void
udf_node_sanity_check(struct udf_node *udf_node,
		uint64_t *cnt_inflen, uint64_t *cnt_logblksrec) {
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct icb_tag *icbtag;
	struct short_ad *short_ad;
	struct long_ad  *long_ad;
	uint64_t inflen, logblksrec;
	uint32_t icbflags, addr_type, max_l_ad;
	uint32_t len, lb_num;
	uint8_t  *data_pos;
	int part_num;
	int adlen, ad_off, dscr_size, l_ea, l_ad, lb_size, flags, whole_lb;

	/* only lock mutex; we're not changing and its a debug checking func */
	mutex_enter(&udf_node->node_mutex);

	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag = &fe->icbtag;
		inflen = udf_rw64(fe->inf_len);
		logblksrec = udf_rw64(fe->logblks_rec);
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
		data_pos = (uint8_t *) fe + dscr_size + l_ea;
	} else {
		icbtag = &efe->icbtag;
		inflen = udf_rw64(efe->inf_len);
		logblksrec = udf_rw64(efe->logblks_rec);
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
		data_pos = (uint8_t *) efe + dscr_size + l_ea;
	}
	max_l_ad = lb_size - dscr_size - l_ea;
	icbflags   = udf_rw16(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/* reset counters */
	*cnt_inflen     = 0;
	*cnt_logblksrec = 0;

	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		KASSERT(l_ad <= max_l_ad);
		KASSERT(l_ad == inflen);
		*cnt_inflen = inflen;
		mutex_exit(&udf_node->node_mutex);
		return;
	}

	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		adlen = sizeof(struct short_ad);
	} else {
		adlen = sizeof(struct long_ad);
	}

	/* start counting */
	whole_lb = 1;
	for (ad_off = 0; ad_off < l_ad; ad_off += adlen) {
		KASSERT(whole_lb == 1);
		if (addr_type == UDF_ICB_SHORT_ALLOC) {
			short_ad = (struct short_ad *) (data_pos + ad_off);
			len      = udf_rw32(short_ad->len);
			lb_num   = udf_rw32(short_ad->lb_num); 
			part_num = -1;
			flags = UDF_EXT_FLAGS(len);
			len   = UDF_EXT_LEN(len);
		} else {
			long_ad  = (struct long_ad *) (data_pos + ad_off);
			len      = udf_rw32(long_ad->len);
			lb_num   = udf_rw32(long_ad->loc.lb_num);
			part_num = udf_rw16(long_ad->loc.part_num);
			flags = UDF_EXT_FLAGS(len);
			len   = UDF_EXT_LEN(len);
		}
		KASSERT(flags != UDF_EXT_REDIRECT);	/* not implemented yet */
		*cnt_inflen += len;
		if (flags == UDF_EXT_ALLOCATED) {
			*cnt_logblksrec += (len + lb_size -1) / lb_size;
		}
		whole_lb = ((len % lb_size) == 0);
	}
	/* rest should be zero (ad_off > l_ad < max_l_ad - adlen) */

	KASSERT(*cnt_inflen == inflen);
	KASSERT(*cnt_logblksrec == logblksrec);

	mutex_exit(&udf_node->node_mutex);
	if (0)
		udf_node_dump(udf_node);
}
#else
#define udf_node_sanity_check(a, b, c)
#endif

/* --------------------------------------------------------------------- */

int
udf_translate_vtop(struct udf_mount *ump, struct long_ad *icb_loc,
		   uint32_t *lb_numres, uint32_t *extres)
{
	struct part_desc       *pdesc;
	struct spare_map_entry *sme;
	struct long_ad s_icb_loc;
	uint64_t foffset, end_foffset;
	uint32_t lb_size, len;
	uint32_t lb_num, lb_rel, lb_packet;
	uint32_t udf_rw32_lbmap, ext_offset;
	uint16_t vpart;
	int rel, part, error, eof, slot, flags;

	assert(ump && icb_loc && lb_numres);

	vpart  = udf_rw16(icb_loc->loc.part_num);
	lb_num = udf_rw32(icb_loc->loc.lb_num);
	if (vpart > UDF_VTOP_RAWPART)
		return EINVAL;

translate_again:
	part = ump->vtop[vpart];
	pdesc = ump->partitions[part];

	switch (ump->vtop_tp[vpart]) {
	case UDF_VTOP_TYPE_RAW :
		/* 1:1 to the end of the device */
		*lb_numres = lb_num;
		*extres = INT_MAX;
		return 0;
	case UDF_VTOP_TYPE_PHYS :
		/* transform into its disc logical block */
		if (lb_num > udf_rw32(pdesc->part_len))
			return EINVAL;
		*lb_numres = lb_num + udf_rw32(pdesc->start_loc);

		/* extent from here to the end of the partition */
		*extres = udf_rw32(pdesc->part_len) - lb_num;
		return 0;
	case UDF_VTOP_TYPE_VIRT :
		/* only maps one logical block, lookup in VAT */
		if (lb_num >= ump->vat_entries)		/* XXX > or >= ? */
			return EINVAL;

		/* lookup in virtual allocation table file */
		mutex_enter(&ump->allocate_mutex);
		error = udf_vat_read(ump->vat_node,
				(uint8_t *) &udf_rw32_lbmap, 4,
				ump->vat_offset + lb_num * 4);
		mutex_exit(&ump->allocate_mutex);

		if (error)
			return error;

		lb_num = udf_rw32(udf_rw32_lbmap);

		/* transform into its disc logical block */
		if (lb_num > udf_rw32(pdesc->part_len))
			return EINVAL;
		*lb_numres = lb_num + udf_rw32(pdesc->start_loc);

		/* just one logical block */
		*extres = 1;
		return 0;
	case UDF_VTOP_TYPE_SPARABLE :
		/* check if the packet containing the lb_num is remapped */
		lb_packet = lb_num / ump->sparable_packet_size;
		lb_rel    = lb_num % ump->sparable_packet_size;

		for (rel = 0; rel < udf_rw16(ump->sparing_table->rt_l); rel++) {
			sme = &ump->sparing_table->entries[rel];
			if (lb_packet == udf_rw32(sme->org)) {
				/* NOTE maps to absolute disc logical block! */
				*lb_numres = udf_rw32(sme->map) + lb_rel;
				*extres    = ump->sparable_packet_size - lb_rel;
				return 0;
			}
		}

		/* transform into its disc logical block */
		if (lb_num > udf_rw32(pdesc->part_len))
			return EINVAL;
		*lb_numres = lb_num + udf_rw32(pdesc->start_loc);

		/* rest of block */
		*extres = ump->sparable_packet_size - lb_rel;
		return 0;
	case UDF_VTOP_TYPE_META :
		/* we have to look into the file's allocation descriptors */

		/* use metadatafile allocation mutex */
		lb_size = udf_rw32(ump->logical_vol->lb_size);

		UDF_LOCK_NODE(ump->metadata_node, 0);

		/* get first overlapping extent */
		foffset = 0;
		slot    = 0;
		for (;;) {
			udf_get_adslot(ump->metadata_node,
				slot, &s_icb_loc, &eof);
			if (eof) {
				DPRINTF(TRANSLATE,
					("Meta partition translation "
					 "failed: can't seek location\n"));
				UDF_UNLOCK_NODE(ump->metadata_node, 0);
				return EINVAL;
			}
			len   = udf_rw32(s_icb_loc.len);
			flags = UDF_EXT_FLAGS(len);
			len   = UDF_EXT_LEN(len);

			end_foffset = foffset + len;

			if (end_foffset > lb_num * lb_size)
				break;	/* found */
			if (flags != UDF_EXT_REDIRECT)
				foffset = end_foffset;
			slot++;
		}
		/* found overlapping slot */
		ext_offset = lb_num * lb_size - foffset;

		/* process extent offset */
		lb_num   = udf_rw32(s_icb_loc.loc.lb_num);
		vpart    = udf_rw16(s_icb_loc.loc.part_num);
		lb_num  += (ext_offset + lb_size -1) / lb_size;
		len     -= ext_offset;
		ext_offset = 0;

		flags = UDF_EXT_FLAGS(s_icb_loc.len);

		UDF_UNLOCK_NODE(ump->metadata_node, 0);
		if (flags != UDF_EXT_ALLOCATED) {
			DPRINTF(TRANSLATE, ("Metadata partition translation "
					    "failed: not allocated\n"));
			return EINVAL;
		}

		/*
		 * vpart and lb_num are updated, translate again since we
		 * might be mapped on sparable media
		 */
		goto translate_again;
	default:
		printf("UDF vtop translation scheme %d unimplemented yet\n",
			ump->vtop_tp[vpart]);
	}

	return EINVAL;
}

/* --------------------------------------------------------------------- */

/*
 * Translate an extent (in logical_blocks) into logical block numbers; used
 * for read and write operations. DOESNT't check extents.
 */

int
udf_translate_file_extent(struct udf_node *udf_node,
		          uint32_t from, uint32_t num_lb,
			  uint64_t *map)
{
	struct udf_mount *ump;
	struct icb_tag *icbtag;
	struct long_ad t_ad, s_ad;
	uint64_t transsec;
	uint64_t foffset, end_foffset;
	uint32_t transsec32;
	uint32_t lb_size;
	uint32_t ext_offset;
	uint32_t lb_num, len;
	uint32_t overlap, translen;
	uint16_t vpart_num;
	int eof, error, flags;
	int slot, addr_type, icbflags;

	if (!udf_node)
		return ENOENT;

	KASSERT(num_lb > 0);

	UDF_LOCK_NODE(udf_node, 0);

	/* initialise derivative vars */
	ump = udf_node->ump;
	lb_size = udf_rw32(ump->logical_vol->lb_size);

	if (udf_node->fe) {
		icbtag = &udf_node->fe->icbtag;
	} else {
		icbtag = &udf_node->efe->icbtag;
	}
	icbflags  = udf_rw16(icbtag->flags);
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/* do the work */
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		*map = UDF_TRANS_INTERN;
		UDF_UNLOCK_NODE(udf_node, 0);
		return 0;
	}

	/* find first overlapping extent */
	foffset = 0;
	slot    = 0;
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof) {
			DPRINTF(TRANSLATE,
				("Translate file extent "
				 "failed: can't seek location\n"));
			UDF_UNLOCK_NODE(udf_node, 0);
			return EINVAL;
		}
		len    = udf_rw32(s_ad.len);
		flags  = UDF_EXT_FLAGS(len);
		len    = UDF_EXT_LEN(len);
		lb_num = udf_rw32(s_ad.loc.lb_num);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		end_foffset = foffset + len;

		if (end_foffset > from * lb_size)
			break;	/* found */
		foffset = end_foffset;
		slot++;
	}
	/* found overlapping slot */
	ext_offset = from * lb_size - foffset;

	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof) {
			DPRINTF(TRANSLATE,
				("Translate file extent "
				 "failed: past eof\n"));
			UDF_UNLOCK_NODE(udf_node, 0);
			return EINVAL;
		}
	
		len    = udf_rw32(s_ad.len);
		flags  = UDF_EXT_FLAGS(len);
		len    = UDF_EXT_LEN(len);

		lb_num    = udf_rw32(s_ad.loc.lb_num);
		vpart_num = udf_rw16(s_ad.loc.part_num);

		end_foffset = foffset + len;

		/* process extent, don't forget to advance on ext_offset! */
		lb_num  += (ext_offset + lb_size -1) / lb_size;
		overlap  = (len - ext_offset + lb_size -1) / lb_size;
		ext_offset = 0;

		/*
		 * note that the while(){} is nessisary for the extent that
		 * the udf_translate_vtop() returns doens't have to span the
		 * whole extent.
		 */
	
		overlap = MIN(overlap, num_lb);
		while (overlap) {
			switch (flags) {
			case UDF_EXT_FREE :
			case UDF_EXT_ALLOCATED_BUT_NOT_USED :
				transsec = UDF_TRANS_ZERO;
				translen = overlap;
				while (overlap && num_lb && translen) {
					*map++ = transsec;
					lb_num++;
					overlap--; num_lb--; translen--;
				}
				break;
			case UDF_EXT_ALLOCATED :
				t_ad.loc.lb_num   = udf_rw32(lb_num);
				t_ad.loc.part_num = udf_rw16(vpart_num);
				error = udf_translate_vtop(ump,
						&t_ad, &transsec32, &translen);
				transsec = transsec32;
				if (error) {
					UDF_UNLOCK_NODE(udf_node, 0);
					return error;
				}
				while (overlap && num_lb && translen) {
					*map++ = transsec;
					lb_num++; transsec++;
					overlap--; num_lb--; translen--;
				}
				break;
			default: /* UDF_EXT_REDIRECT */
				/* ignore, not a mapping */
				break;
			}
		}
		if (num_lb == 0)
			break;

		if (flags != UDF_EXT_REDIRECT)
			foffset = end_foffset;
		slot++;
	}
	UDF_UNLOCK_NODE(udf_node, 0);

	return 0;
}

/* --------------------------------------------------------------------- */

static int
udf_search_free_vatloc(struct udf_mount *ump, uint32_t *lbnumres)
{
	uint32_t lb_size, lb_num, lb_map, udf_rw32_lbmap;
	uint8_t *blob;
	int entry, chunk, found, error;

	KASSERT(ump);
	KASSERT(ump->logical_vol);

	lb_size = udf_rw32(ump->logical_vol->lb_size);
	blob = malloc(lb_size, M_UDFTEMP, M_WAITOK);

	/* TODO static allocation of search chunk */

	lb_num = MIN(ump->vat_entries, ump->vat_last_free_lb);
	found  = 0;
	error  = 0;
	entry  = 0;
	do {
		chunk = MIN(lb_size, (ump->vat_entries - lb_num) * 4);
		if (chunk <= 0)
			break;
		/* load in chunk */
		error = udf_vat_read(ump->vat_node, blob, chunk,
				ump->vat_offset + lb_num * 4);

		if (error)
			break;

		/* search this chunk */
		for (entry=0; entry < chunk /4; entry++, lb_num++) {
			udf_rw32_lbmap = *((uint32_t *) (blob + entry * 4));
			lb_map = udf_rw32(udf_rw32_lbmap);
			if (lb_map == 0xffffffff) {
				found = 1;
				break;
			}
		}
	} while (!found);
	if (error) {
		printf("udf_search_free_vatloc: error reading in vat chunk "
			"(lb %d, size %d)\n", lb_num, chunk);
	}

	if (!found) {
		/* extend VAT */
		DPRINTF(WRITE, ("udf_search_free_vatloc: extending\n"));
		lb_num = ump->vat_entries;
		ump->vat_entries++;
	}

	/* mark entry with initialiser just in case */
	lb_map = udf_rw32(0xfffffffe);
	udf_vat_write(ump->vat_node, (uint8_t *) &lb_map, 4,
		ump->vat_offset + lb_num *4);
	ump->vat_last_free_lb = lb_num;

	free(blob, M_UDFTEMP);
	*lbnumres = lb_num;
	return 0;
}


static void
udf_bitmap_allocate(struct udf_bitmap *bitmap, int ismetadata,
	uint32_t ptov, uint32_t *num_lb, uint64_t *pmappos, uint64_t *lmappos)
{
	uint32_t offset, lb_num, bit;
	int32_t  diff;
	uint8_t *bpos;
	int pass;

	if (!ismetadata) {
		/* heuristic to keep the two pointers not too close */
		diff = bitmap->data_pos - bitmap->metadata_pos;
		if ((diff >= 0) && (diff < 1024))
			bitmap->data_pos = bitmap->metadata_pos + 1024;
	}
	offset = ismetadata ? bitmap->metadata_pos : bitmap->data_pos;
	offset &= ~7;
	for (pass = 0; pass < 2; pass++) {
		if (offset >= bitmap->max_offset)
			offset = 0;

		while (offset < bitmap->max_offset) {
			if (*num_lb == 0)
				break;

			/* use first bit not set */
			bpos  = bitmap->bits + offset/8;
			bit = ffs(*bpos);
			if (bit == 0) {
				offset += 8;
				continue;
			}
			*bpos &= ~(1 << (bit-1));
			lb_num = offset + bit-1;
			*lmappos++ = lb_num;
			*pmappos++ = lb_num + ptov;
			*num_lb = *num_lb - 1;
			// offset = (offset & ~7);
		}
	}

	if (ismetadata) {
		bitmap->metadata_pos = offset;
	} else {
		bitmap->data_pos = offset;
	}
}


static void
udf_bitmap_free(struct udf_bitmap *bitmap, uint32_t lb_num, uint32_t num_lb)
{
	uint32_t offset;
	uint32_t bit, bitval;
	uint8_t *bpos;

	offset = lb_num;

	/* starter bits */
	bpos = bitmap->bits + offset/8;
	bit = offset % 8;
	while ((bit != 0) && (num_lb > 0)) {
		bitval = (1 << bit);
		KASSERT((*bpos & bitval) == 0);
		*bpos |= bitval;
		offset++; num_lb--;
		bit = (bit + 1) % 8;
	}
	if (num_lb == 0)
		return;

	/* whole bytes */
	KASSERT(bit == 0);
	bpos = bitmap->bits + offset / 8;
	while (num_lb >= 8) {
		KASSERT((*bpos == 0));
		*bpos = 255;
		offset += 8; num_lb -= 8;
		bpos++;
	}

	/* stop bits */
	KASSERT(num_lb < 8);
	bit = 0;
	while (num_lb > 0) {
		bitval = (1 << bit);
		KASSERT((*bpos & bitval) == 0);
		*bpos |= bitval;
		offset++; num_lb--;
		bit = (bit + 1) % 8;
	}
}


/* allocate a contiguous sequence of sectornumbers */
static int
udf_allocate_space(struct udf_mount *ump, int ismetadata, int alloc_type,
	int num_lb, uint16_t *alloc_partp,
	uint64_t *lmapping, uint64_t *pmapping)
{
	struct mmc_trackinfo *alloc_track, *other_track;
	struct udf_bitmap *bitmap;
	struct part_desc *pdesc;
	struct logvol_int_desc *lvid;
	uint64_t *lmappos, *pmappos;
	uint32_t ptov, lb_num, *freepos, free_lbs;
	int lb_size, alloc_num_lb;
	int alloc_part;
	int error;

	mutex_enter(&ump->allocate_mutex);
	
	lb_size = udf_rw32(ump->logical_vol->lb_size);
	KASSERT(lb_size == ump->discinfo.sector_size);

	if (ismetadata) {
		alloc_part  = ump->metadata_part;
		alloc_track = &ump->metadata_track;
		other_track = &ump->data_track;
	} else {
		alloc_part  = ump->data_part;
		alloc_track = &ump->data_track;
		other_track = &ump->metadata_track;
	}

	*alloc_partp = alloc_part;

	error = 0;
	/* XXX check disc space */

	pdesc = ump->partitions[ump->vtop[alloc_part]];
	lmappos = lmapping;
	pmappos = pmapping;

	switch (alloc_type) {
	case UDF_ALLOC_VAT :
		/* search empty slot in VAT file */
		KASSERT(num_lb == 1);
		error = udf_search_free_vatloc(ump, &lb_num);
		if (!error) {
			*lmappos = lb_num;
			*pmappos = 0;		/* will get late-allocated */
		}
		break;
	case UDF_ALLOC_SEQUENTIAL :
		/* sequential allocation on recordable media */
		/* calculate offset from physical base partition */
		ptov  = udf_rw32(pdesc->start_loc);

		for (lb_num = 0; lb_num < num_lb; lb_num++) {
			*pmappos++ = alloc_track->next_writable;
			*lmappos++ = alloc_track->next_writable - ptov;
			alloc_track->next_writable++;
			alloc_track->free_blocks--;
		}
		if (alloc_track->tracknr == other_track->tracknr)
			memcpy(other_track, alloc_track,
				sizeof(struct mmc_trackinfo));
		break;
	case UDF_ALLOC_SPACEMAP :
		ptov  = udf_rw32(pdesc->start_loc);

		/* allocate on unallocated bits page */
		alloc_num_lb = num_lb;
		bitmap = &ump->part_unalloc_bits[alloc_part];
		udf_bitmap_allocate(bitmap, ismetadata, ptov, &alloc_num_lb,
			pmappos, lmappos);
		ump->lvclose |= UDF_WRITE_PART_BITMAPS;
		if (alloc_num_lb) {
			/* TODO convert freed to unalloc and try again */
			/* free allocated piece for now */
			lmappos = lmapping;
			for (lb_num=0; lb_num < num_lb-alloc_num_lb; lb_num++) {
				udf_bitmap_free(bitmap, *lmappos++, 1);
			}
			error = ENOSPC;
		}
		if (!error) {
			/* adjust freecount */
			lvid = ump->logvol_integrity;
			freepos = &lvid->tables[0] + alloc_part;
			free_lbs = udf_rw32(*freepos);
			*freepos = udf_rw32(free_lbs - num_lb);
		}
		break;
	case UDF_ALLOC_METABITMAP :
	case UDF_ALLOC_METASEQUENTIAL :
	case UDF_ALLOC_RELAXEDSEQUENTIAL :
		printf("ALERT: udf_allocate_space : allocation %d "
				"not implemented yet!\n", alloc_type);
		/* TODO implement, doesn't have to be contiguous */
		error = ENOSPC;
		break;
	}

#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_ALLOC) {
		lmappos = lmapping;
		pmappos = pmapping;
		printf("udf_allocate_space, mapping l->p:\n");
		for (lb_num = 0; lb_num < num_lb; lb_num++) {
			printf("\t%"PRIu64" -> %"PRIu64"\n",
				*lmappos++, *pmappos++);
		}
	}
#endif
	mutex_exit(&ump->allocate_mutex);

	return error;
}

/* --------------------------------------------------------------------- */

void
udf_free_allocated_space(struct udf_mount *ump, uint32_t lb_num,
	uint16_t vpart_num, uint32_t num_lb)
{
	struct udf_bitmap *bitmap;
	struct part_desc *pdesc;
	struct logvol_int_desc *lvid;
	uint32_t ptov, lb_map, udf_rw32_lbmap;
	uint32_t *freepos, free_lbs;
	int phys_part;
	int error;

	DPRINTF(ALLOC, ("udf_free_allocated_space: freeing virt lbnum %d "
			  "part %d + %d sect\n", lb_num, vpart_num, num_lb));

	mutex_enter(&ump->allocate_mutex);

	/* get partition backing up this vpart_num */
	pdesc = ump->partitions[ump->vtop[vpart_num]];

	switch (ump->vtop_tp[vpart_num]) {
	case UDF_VTOP_TYPE_PHYS :
	case UDF_VTOP_TYPE_SPARABLE :
		/* free space to freed or unallocated space bitmap */
		ptov      = udf_rw32(pdesc->start_loc);
		phys_part = ump->vtop[vpart_num];

		/* first try freed space bitmap */
		bitmap    = &ump->part_freed_bits[phys_part];

		/* if not defined, use unallocated bitmap */
		if (bitmap->bits == NULL)
			bitmap = &ump->part_unalloc_bits[phys_part];

		/* if no bitmaps are defined, bail out */
		if (bitmap->bits == NULL)
			break;

		/* free bits if its defined */
		KASSERT(bitmap->bits);
		ump->lvclose |= UDF_WRITE_PART_BITMAPS;
		udf_bitmap_free(bitmap, lb_num, num_lb);

		/* adjust freecount */
		lvid = ump->logvol_integrity;
		freepos = &lvid->tables[0] + vpart_num;
		free_lbs = udf_rw32(*freepos);
		*freepos = udf_rw32(free_lbs + num_lb);
		break;
	case UDF_VTOP_TYPE_VIRT :
		/* free this VAT entry */
		KASSERT(num_lb == 1);

		lb_map = 0xffffffff;
		udf_rw32_lbmap = udf_rw32(lb_map);
		error = udf_vat_write(ump->vat_node,
			(uint8_t *) &udf_rw32_lbmap, 4,
			ump->vat_offset + lb_num * 4);
		KASSERT(error == 0);
		ump->vat_last_free_lb = MIN(ump->vat_last_free_lb, lb_num);
		break;
	case UDF_VTOP_TYPE_META :
		/* free space in the metadata bitmap */
	default:
		printf("ALERT: udf_free_allocated_space : allocation %d "
			"not implemented yet!\n", ump->vtop_tp[vpart_num]);
		break;
	}

	mutex_exit(&ump->allocate_mutex);
}

/* --------------------------------------------------------------------- */

int
udf_pre_allocate_space(struct udf_mount *ump, int udf_c_type, int num_lb,
	uint16_t *alloc_partp, uint64_t *lmapping, uint64_t *pmapping)
{
	int ismetadata, alloc_type;

	ismetadata = (udf_c_type == UDF_C_NODE);
	alloc_type = ismetadata? ump->meta_alloc : ump->data_alloc;

#ifdef DIAGNOSTIC
	if ((alloc_type == UDF_ALLOC_VAT) && (udf_c_type != UDF_C_NODE)) {
		panic("udf_pre_allocate_space: bad c_type on VAT!\n");
	}
#endif

	/* reserve size for VAT allocated data */
	if (alloc_type == UDF_ALLOC_VAT) {
		mutex_enter(&ump->allocate_mutex);
			ump->uncomitted_lb += num_lb;
		mutex_exit(&ump->allocate_mutex);
	}

	return udf_allocate_space(ump, ismetadata, alloc_type,
		num_lb, alloc_partp, lmapping, pmapping);
}

/* --------------------------------------------------------------------- */

/*
 * Allocate a buf on disc for direct write out. The space doesn't have to be
 * contiguous as the caller takes care of this.
 */

void
udf_late_allocate_buf(struct udf_mount *ump, struct buf *buf,
	uint64_t *lmapping, uint64_t *pmapping, struct long_ad *node_ad_cpy)
{
	struct udf_node  *udf_node = VTOI(buf->b_vp);
	uint16_t vpart_num;
	int lb_size, blks, udf_c_type;
	int ismetadata, alloc_type;
	int num_lb;
	int error, s;

	/*
	 * for each sector in the buf, allocate a sector on disc and record
	 * its position in the provided mapping array.
	 *
	 * If its userdata or FIDs, record its location in its node.
	 */

	lb_size    = udf_rw32(ump->logical_vol->lb_size);
	num_lb     = (buf->b_bcount + lb_size -1) / lb_size;
	blks       = lb_size / DEV_BSIZE;
	udf_c_type = buf->b_udf_c_type;

	KASSERT(lb_size == ump->discinfo.sector_size);

	ismetadata = (udf_c_type == UDF_C_NODE);
	alloc_type = ismetadata? ump->meta_alloc : ump->data_alloc;

#ifdef DIAGNOSTIC
	if ((alloc_type == UDF_ALLOC_VAT) && (udf_c_type != UDF_C_NODE)) {
		panic("udf_late_allocate_buf: bad c_type on VAT!\n");
	}
#endif

	if (udf_c_type == UDF_C_NODE) {
		/* if not VAT, its allready allocated */
		if (alloc_type != UDF_ALLOC_VAT)
			return;

		/* allocate sequential */
		alloc_type = UDF_ALLOC_SEQUENTIAL;
	}

	error = udf_allocate_space(ump, ismetadata, alloc_type,
			num_lb, &vpart_num, lmapping, pmapping);
	if (error) {
		/* ARGH! we've not done our accounting right! */
		panic("UDF disc allocation accounting gone wrong");
	}

	/* commit our sector count */
	mutex_enter(&ump->allocate_mutex);
		if (num_lb > ump->uncomitted_lb) {
			ump->uncomitted_lb = 0;
		} else {
			ump->uncomitted_lb -= num_lb;
		}
	mutex_exit(&ump->allocate_mutex);

	buf->b_blkno = (*pmapping) * blks;

	/* If its userdata or FIDs, record its allocation in its node. */
	if ((udf_c_type == UDF_C_USERDATA) || (udf_c_type == UDF_C_FIDS)) {
		udf_record_allocation_in_node(ump, buf, vpart_num, lmapping,
			node_ad_cpy);
		/* decrement our outstanding bufs counter */
		s = splbio();
			udf_node->outstanding_bufs--;
		splx(s);
	}
}

/* --------------------------------------------------------------------- */

/*
 * Try to merge a1 with the new piece a2. udf_ads_merge returns error when not
 * possible (anymore); a2 returns the rest piece.
 */

static int
udf_ads_merge(uint32_t lb_size, struct long_ad *a1, struct long_ad *a2)
{
	uint32_t max_len, merge_len;
	uint32_t a1_len, a2_len;
	uint32_t a1_flags, a2_flags;
	uint32_t a1_lbnum, a2_lbnum;
	uint16_t a1_part, a2_part;

	max_len = ((UDF_EXT_MAXLEN / lb_size) * lb_size);

	a1_flags = UDF_EXT_FLAGS(udf_rw32(a1->len));
	a1_len   = UDF_EXT_LEN(udf_rw32(a1->len));
	a1_lbnum = udf_rw32(a1->loc.lb_num);
	a1_part  = udf_rw16(a1->loc.part_num);

	a2_flags = UDF_EXT_FLAGS(udf_rw32(a2->len));
	a2_len   = UDF_EXT_LEN(udf_rw32(a2->len));
	a2_lbnum = udf_rw32(a2->loc.lb_num);
	a2_part  = udf_rw16(a2->loc.part_num);

	/* defines same space */
	if (a1_flags != a2_flags)
		return 1;

	if (a1_flags != UDF_EXT_FREE) {
		/* the same partition */
		if (a1_part != a2_part)
			return 1;

		/* a2 is successor of a1 */
		if (a1_lbnum * lb_size + a1_len != a2_lbnum * lb_size)
			return 1;
	}
	
	/* merge as most from a2 if possible */
	merge_len = MIN(a2_len, max_len - a1_len);
	a1_len   += merge_len;
	a2_len   -= merge_len;
	a2_lbnum += merge_len/lb_size;

	a1->len = udf_rw32(a1_len | a1_flags);
	a2->len = udf_rw32(a2_len | a2_flags);
	a2->loc.lb_num = udf_rw32(a2_lbnum);

	if (a2_len > 0)
		return 1;

	/* there is space over to merge */
	return 0;
}

/* --------------------------------------------------------------------- */

static void
udf_wipe_adslots(struct udf_node *udf_node)
{
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct alloc_ext_entry *ext;
	uint64_t inflen, objsize;
	uint32_t lb_size, dscr_size, l_ea, l_ad, max_l_ad, crclen;
	uint8_t *data_pos;
	int extnr;

	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		inflen  = udf_rw64(fe->inf_len);
		objsize = inflen;
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
		data_pos = (uint8_t *) fe + dscr_size + l_ea;
	} else {
		inflen  = udf_rw64(efe->inf_len);
		objsize = udf_rw64(efe->obj_size);
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
		data_pos = (uint8_t *) efe + dscr_size + l_ea;
	}
	max_l_ad = lb_size - dscr_size - l_ea;

	/* wipe fe/efe */
	memset(data_pos, 0, max_l_ad);
	crclen = dscr_size - UDF_DESC_TAG_LENGTH + l_ea;
	if (fe) {
		fe->l_ad         = udf_rw32(0);
		fe->logblks_rec  = udf_rw64(0);
		fe->tag.desc_crc_len = udf_rw32(crclen);
	} else {
		efe->l_ad        = udf_rw32(0);
		efe->logblks_rec = udf_rw64(0);
		efe->tag.desc_crc_len = udf_rw32(crclen);
	}

	/* wipe all allocation extent entries */
	for (extnr = 0; extnr < udf_node->num_extensions; extnr++) {
		ext = udf_node->ext[extnr];
		dscr_size  = sizeof(struct alloc_ext_entry) -1;
		max_l_ad = lb_size - dscr_size;
		memset(data_pos, 0, max_l_ad);
		ext->l_ad = udf_rw32(0);

		crclen = dscr_size - UDF_DESC_TAG_LENGTH;
		ext->tag.desc_crc_len = udf_rw32(crclen);
	}
}

/* --------------------------------------------------------------------- */

void
udf_get_adslot(struct udf_node *udf_node, int slot, struct long_ad *icb,
	int *eof) {
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct alloc_ext_entry *ext;
	struct icb_tag *icbtag;
	struct short_ad *short_ad;
	struct long_ad *long_ad;
	uint32_t offset;
	uint32_t lb_size, dscr_size, l_ea, l_ad, max_l_ad;
	uint8_t *data_pos;
	int icbflags, addr_type, adlen, extnr;

	/* determine what descriptor we are in */
	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag  = &fe->icbtag;
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
		data_pos = (uint8_t *) fe + dscr_size + l_ea;
	} else {
		icbtag  = &efe->icbtag;
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
		data_pos = (uint8_t *) efe + dscr_size + l_ea;
	}
	max_l_ad = lb_size - dscr_size - l_ea;

	icbflags  = udf_rw16(icbtag->flags);
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/* just in case we're called on an intern, its EOF */
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		memset(icb, 0, sizeof(struct long_ad));
		*eof = 1;
		return;
	}

	adlen = 0;
	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		adlen = sizeof(struct short_ad);
	} else if (addr_type == UDF_ICB_LONG_ALLOC) {
		adlen = sizeof(struct long_ad);
	}

	/* if offset too big, we go to the allocation extensions */
	offset = slot * adlen;
	extnr  = 0;
	while (offset > max_l_ad) {
		offset -= max_l_ad;
		ext  = udf_node->ext[extnr];
		dscr_size  = sizeof(struct alloc_ext_entry) -1;
		l_ad = udf_rw32(ext->l_ad);
		max_l_ad = lb_size - dscr_size;
		data_pos = (uint8_t *) ext + dscr_size + l_ea;
		extnr++;
		if (extnr > udf_node->num_extensions) {
			l_ad = 0;	/* force EOF */
			break;
		}
	}

	*eof = (offset >= l_ad) || (l_ad == 0);
	if (*eof) {
		memset(icb, 0, sizeof(struct long_ad));
		return;
	}

	/* get the element */
	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		short_ad = (struct short_ad *) (data_pos + offset);
		icb->len          = short_ad->len;
		icb->loc.part_num = udf_rw16(0);	/* ignore */
		icb->loc.lb_num   = short_ad->lb_num;
	} else if (addr_type == UDF_ICB_LONG_ALLOC) {
		long_ad = (struct long_ad *) (data_pos + offset);
		*icb = *long_ad;
	}
}

/* --------------------------------------------------------------------- */

int
udf_append_adslot(struct udf_node *udf_node, int slot, struct long_ad *icb) {
	union dscrptr          *dscr;
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct alloc_ext_entry *ext;
	struct icb_tag *icbtag;
	struct short_ad *short_ad;
	struct long_ad *long_ad, o_icb;
	uint64_t logblks_rec, *logblks_rec_p;
	uint32_t offset, rest, len;
	uint32_t lb_size, dscr_size, l_ea, l_ad, *l_ad_p, max_l_ad, crclen;
	uint8_t *data_pos;
	int icbflags, addr_type, adlen, extnr;

	/* determine what descriptor we are in */
	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag  = &fe->icbtag;
		dscr      = (union dscrptr *) fe;
		dscr_size = sizeof(struct file_entry) -1;

		l_ea      = udf_rw32(fe->l_ea);
		l_ad_p    = &fe->l_ad;
		logblks_rec_p = &fe->logblks_rec;
	} else {
		icbtag    = &efe->icbtag;
		dscr      = (union dscrptr *) efe;
		dscr_size = sizeof(struct extfile_entry) -1;

		l_ea      = udf_rw32(efe->l_ea);
		l_ad_p    = &efe->l_ad;
		logblks_rec_p = &efe->logblks_rec;
	}
	data_pos  = (uint8_t *) dscr + dscr_size + l_ea;
	max_l_ad = lb_size - dscr_size - l_ea;

	icbflags  = udf_rw16(icbtag->flags);
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	/* just in case we're called on an intern, its EOF */
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		panic("udf_append_adslot on UDF_ICB_INTERN_ALLOC\n");
	}

	adlen = 0;
	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		adlen = sizeof(struct short_ad);
	} else if (addr_type == UDF_ICB_LONG_ALLOC) {
		adlen = sizeof(struct long_ad);
	}

	/* if offset too big, we go to the allocation extensions */
	offset = slot * adlen;
	extnr  = 0;
	while (offset > max_l_ad) {
		offset -= max_l_ad;
		ext  = udf_node->ext[extnr];
		dscr = (union dscrptr *) ext;
		dscr_size  = sizeof(struct alloc_ext_entry) -1;

		KASSERT(ext != NULL);
		l_ad_p = &ext->l_ad;
		max_l_ad = lb_size - dscr_size;
		data_pos = (uint8_t *) dscr + dscr_size;

		extnr++;
	}
	/* offset is offset within the current (E)FE/AED */
	l_ad   = udf_rw32(*l_ad_p);
	crclen = udf_rw32(dscr->tag.desc_crc_len);
	logblks_rec = udf_rw64(*logblks_rec_p);

	if (extnr > udf_node->num_extensions)
		return EFBIG;	/* too fragmented */

	/* overwriting old piece? */
	if (offset < l_ad) {
		/* overwrite entry; compensate for the old element */
		if (addr_type == UDF_ICB_SHORT_ALLOC) {
			short_ad = (struct short_ad *) (data_pos + offset);
			o_icb.len          = short_ad->len;
			o_icb.loc.part_num = udf_rw16(0);	/* ignore */
			o_icb.loc.lb_num   = short_ad->lb_num;
		} else if (addr_type == UDF_ICB_LONG_ALLOC) {
			long_ad = (struct long_ad *) (data_pos + offset);
			o_icb = *long_ad;
		} else {
			panic("Invalid address type in udf_append_adslot\n");
		}

		len = udf_rw32(o_icb.len);
		if (UDF_EXT_FLAGS(len) == UDF_EXT_ALLOCATED) {
			/* adjust counts */
			len = UDF_EXT_LEN(len);
			logblks_rec -= (len + lb_size -1) / lb_size;
		}
	}

	/* calculate rest space in this descriptor */
	rest = max_l_ad - offset;
	if (rest <= adlen) {
		/* create redirect and link new allocation extension */
		printf("udf_append_to_adslot: can't create allocation extention yet\n");
		return EFBIG;
	}

	/* write out the element */
	if (addr_type == UDF_ICB_SHORT_ALLOC) {
		short_ad = (struct short_ad *) (data_pos + offset);
		short_ad->len    = icb->len;
		short_ad->lb_num = icb->loc.lb_num;
	} else if (addr_type == UDF_ICB_LONG_ALLOC) {
		long_ad = (struct long_ad *) (data_pos + offset);
		*long_ad = *icb;
	}

	/* adjust logblks recorded count */
	if (UDF_EXT_FLAGS(icb->len) == UDF_EXT_ALLOCATED)
		logblks_rec += (UDF_EXT_LEN(icb->len) + lb_size -1) / lb_size;
	*logblks_rec_p = udf_rw64(logblks_rec);

	/* adjust l_ad and crclen when needed */
	if (offset >= l_ad) {
		l_ad   += adlen;
		crclen += adlen;
		dscr->tag.desc_crc_len = udf_rw32(crclen);
		*l_ad_p = udf_rw32(l_ad);
	}

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Adjust the node's allocation descriptors to reflect the new mapping; do
 * take note that we might glue to existing allocation descriptors.
 *
 * XXX Note there can only be one allocation being recorded/mount; maybe
 * explicit allocation in shedule thread?
 */

static void
udf_record_allocation_in_node(struct udf_mount *ump, struct buf *buf,
	uint16_t vpart_num, uint64_t *mapping, struct long_ad *node_ad_cpy)
{
	struct vnode    *vp = buf->b_vp;
	struct udf_node *udf_node = VTOI(vp);
	struct file_entry      *fe;
	struct extfile_entry   *efe;
	struct icb_tag  *icbtag;
	struct long_ad   s_ad, c_ad;
	uint64_t inflen, from, till;
	uint64_t foffset, end_foffset, restart_foffset;
	uint64_t orig_inflen, orig_lbrec, new_inflen, new_lbrec;
	uint32_t num_lb, len, flags, lb_num;
	uint32_t run_start;
	uint32_t slot_offset;
	uint32_t skip_len, skipped;
	int addr_type, icbflags;
	int udf_c_type = buf->b_udf_c_type;
	int lb_size, run_length, eof;
	int slot, cpy_slot, cpy_slots, restart_slot;
	int error;

	DPRINTF(ALLOC, ("udf_record_allocation_in_node\n"));
	udf_node_sanity_check(udf_node, &orig_inflen, &orig_lbrec);

	/* sanity check ... should be panic ? */
	if ((udf_c_type != UDF_C_USERDATA) && (udf_c_type != UDF_C_FIDS))
		return;

	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	/* do the job */
	UDF_LOCK_NODE(udf_node, 0);	/* XXX can deadlock ? */

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag = &fe->icbtag;
		inflen = udf_rw64(fe->inf_len);
	} else {
		icbtag = &efe->icbtag;
		inflen = udf_rw64(efe->inf_len);
	}

	/* do check if `till' is not past file information length */
	from = buf->b_lblkno * lb_size;
	till = MIN(inflen, from + buf->b_resid);

	num_lb = (till - from + lb_size -1) / lb_size;

	DPRINTF(ALLOC, ("record allocation from = %"PRIu64" + %d\n", from, buf->b_bcount));

	icbflags  = udf_rw16(icbtag->flags);
	addr_type = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		/* nothing to do */
		/* XXX clean up rest of node? just in case? */
		UDF_UNLOCK_NODE(udf_node, 0);
		return;
	}

	slot     = 0;
	cpy_slot = 0;
	foffset  = 0;

	/* 1) copy till first overlap piece to the rewrite buffer */
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof) {
			DPRINTF(WRITE,
				("Record allocation in node "
				 "failed: encountered EOF\n"));
			UDF_UNLOCK_NODE(udf_node, 0);
			buf->b_error = EINVAL;
			return;
		}
		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		end_foffset = foffset + len;
		if (end_foffset > from)
			break;	/* found */

		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t1: vp %d, lb %d, len %d, flags %d "
			"-> stack\n",
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		foffset = end_foffset;
		slot++;
	}
	restart_slot    = slot;
	restart_foffset = foffset;

	/* 2) trunc overlapping slot at overlap and copy it */
	slot_offset = from - foffset;
	if (slot_offset > 0) {
		DPRINTF(ALLOC, ("\tslot_offset = %d, flags = %d (%d)\n",
				slot_offset, flags >> 30, flags));

		s_ad.len = udf_rw32(slot_offset | flags);
		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t2: vp %d, lb %d, len %d, flags %d "
			"-> stack\n",
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));
	}
	foffset += slot_offset;

	/* 3) insert new mappings */
	memset(&s_ad, 0, sizeof(struct long_ad));
	lb_num = 0;
	for (lb_num = 0; lb_num < num_lb; lb_num++) {
		run_start  = mapping[lb_num];
		run_length = 1;
		while (lb_num < num_lb-1) {
			if (mapping[lb_num+1] != mapping[lb_num]+1)
				if (mapping[lb_num+1] != mapping[lb_num])
					break;
			run_length++;
			lb_num++;
		}
		/* insert slot for this mapping */
		len = run_length * lb_size;

		/* bounds checking */
		if (foffset + len > till)
			len = till - foffset;
		KASSERT(foffset + len <= inflen);

		s_ad.len = udf_rw32(len | UDF_EXT_ALLOCATED);
		s_ad.loc.part_num = udf_rw16(vpart_num);
		s_ad.loc.lb_num   = udf_rw32(run_start);

		foffset += len;

		/* paranoia */
		if (len == 0) {
			DPRINTF(WRITE,
				("Record allocation in node "
				 "failed: insert failed\n"));
			UDF_UNLOCK_NODE(udf_node, 0);
			buf->b_error = EINVAL;
			return;
		}
		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t3: insert new mapping vp %d lb %d, len %d, "
				"flags %d -> stack\n",
			udf_rw16(s_ad.loc.part_num), udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));
	}

	/* 4) pop replaced length */
	slot = restart_slot;
	foffset = restart_foffset;

	skip_len = till - foffset;	/* relative to start of slot */
	slot_offset = from - foffset;
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;

		len    = udf_rw32(s_ad.len);
		flags  = UDF_EXT_FLAGS(len);
		len    = UDF_EXT_LEN(len);
		lb_num = udf_rw32(s_ad.loc.lb_num);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		DPRINTF(ALLOC, ("\t4i: got slot %d, skip_len %d, vp %d, "
				"lb %d, len %d, flags %d\n",
			slot, skip_len, udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		skipped   = MIN(len, skip_len);
		if (flags != UDF_EXT_FREE) {
			if (slot_offset) {
				/* skip these blocks first */
				num_lb = (slot_offset + lb_size-1) / lb_size;
				len      -= slot_offset;
				skip_len -= slot_offset;
				foffset  += slot_offset;
				lb_num   += num_lb;
				skipped  -= slot_offset;
				slot_offset = 0;
			}
			/* free space from current position till `skipped' */
			num_lb = (skipped + lb_size-1) / lb_size;
			udf_free_allocated_space(ump, lb_num,
				udf_rw16(s_ad.loc.part_num), num_lb);
			lb_num += num_lb;
		}
		len      -= skipped;
		skip_len -= skipped;
		foffset  += skipped;

		if (len) {
			KASSERT(skipped % lb_size == 0);

			/* we arrived at our point, push remainder */
			s_ad.len        = udf_rw32(len | flags);
			s_ad.loc.lb_num = udf_rw32(lb_num);
			node_ad_cpy[cpy_slot++] = s_ad;
			foffset += len;
			slot++;

			DPRINTF(ALLOC, ("\t4: vp %d, lb %d, len %d, flags %d "
				"-> stack\n",
				udf_rw16(s_ad.loc.part_num),
				udf_rw32(s_ad.loc.lb_num),
				UDF_EXT_LEN(udf_rw32(s_ad.len)),
				UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));
			break;
		}
		slot++;
	}

	/* 5) copy remainder */
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;

		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t5: insert new mapping "
			"vp %d lb %d, len %d, flags %d "
			"-> stack\n",
		udf_rw16(s_ad.loc.part_num),
		udf_rw32(s_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(s_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		slot++;
	}

	/* 6) reset node descriptors */
	udf_wipe_adslots(udf_node);

	/* 7) copy back extents; merge when possible. Recounting on the fly */
	cpy_slots = cpy_slot;

	c_ad = node_ad_cpy[0];
	slot = 0;
	DPRINTF(ALLOC, ("\t7s: stack -> got mapping vp %d "
		"lb %d, len %d, flags %d\n",
	udf_rw16(c_ad.loc.part_num),
	udf_rw32(c_ad.loc.lb_num),
	UDF_EXT_LEN(udf_rw32(c_ad.len)),
	UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

	for (cpy_slot = 1; cpy_slot < cpy_slots; cpy_slot++) {
		s_ad = node_ad_cpy[cpy_slot];

		DPRINTF(ALLOC, ("\t7i: stack -> got mapping vp %d "
			"lb %d, len %d, flags %d\n",
		udf_rw16(s_ad.loc.part_num),
		udf_rw32(s_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(s_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		/* see if we can merge */
		if (udf_ads_merge(lb_size, &c_ad, &s_ad)) {
			/* not mergable (anymore) */
			DPRINTF(ALLOC, ("\t7: appending vp %d lb %d, "
				"len %d, flags %d\n",
			udf_rw16(c_ad.loc.part_num),
			udf_rw32(c_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(c_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

			error = udf_append_adslot(udf_node, slot, &c_ad);
			if (error) {
				buf->b_error = error;
				goto out;
			}
			c_ad = s_ad;
			slot++;
		}
	}

	/* 8) push rest slot (if any) */
	if (UDF_EXT_LEN(c_ad.len) > 0) {
		DPRINTF(ALLOC, ("\t8: last append vp %d lb %d, "
				"len %d, flags %d\n",
		udf_rw16(c_ad.loc.part_num),
		udf_rw32(c_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(c_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

		error = udf_append_adslot(udf_node, slot, &c_ad);
		if (error) {
			buf->b_error = error;
			goto out;
		}
	}

out:
	/* the node's descriptors should now be sane */
	UDF_UNLOCK_NODE(udf_node, 0);

	udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);

	KASSERT(orig_inflen == new_inflen);
	KASSERT(new_lbrec >= orig_lbrec);

	return;
}

/* --------------------------------------------------------------------- */

int
udf_grow_node(struct udf_node *udf_node, uint64_t new_size)
{
	union dscrptr *dscr;
	struct vnode *vp = udf_node->vnode;
	struct udf_mount *ump = udf_node->ump;
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct icb_tag  *icbtag;
	struct long_ad c_ad, s_ad;
	uint64_t size_diff, old_size, inflen, objsize, chunk, append_len;
	uint64_t foffset, end_foffset;
	uint64_t orig_inflen, orig_lbrec, new_inflen, new_lbrec;
	uint32_t lb_size, dscr_size, crclen, lastblock_grow;
	uint32_t len, flags, max_len;
	uint32_t max_l_ad, l_ad, l_ea;
	uint8_t *data_pos, *evacuated_data;
	int icbflags, addr_type;
	int slot, cpy_slot;
	int eof, error;

	DPRINTF(ALLOC, ("udf_grow_node\n"));
	udf_node_sanity_check(udf_node, &orig_inflen, &orig_lbrec);

	UDF_LOCK_NODE(udf_node, 0);
	lb_size = udf_rw32(ump->logical_vol->lb_size);
	max_len = ((UDF_EXT_MAXLEN / lb_size) * lb_size);

	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		dscr       = (union dscrptr *) fe;
		icbtag  = &fe->icbtag;
		inflen  = udf_rw64(fe->inf_len);
		objsize = inflen;
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
	} else {
		dscr       = (union dscrptr *) efe;
		icbtag  = &efe->icbtag;
		inflen  = udf_rw64(efe->inf_len);
		objsize = udf_rw64(efe->obj_size);
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
	}
	data_pos  = (uint8_t *) dscr + dscr_size + l_ea;
	max_l_ad = lb_size - dscr_size - l_ea;

	icbflags   = udf_rw16(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	old_size  = inflen;
	size_diff = new_size - old_size;

	DPRINTF(ALLOC, ("\tfrom %"PRIu64" to %"PRIu64"\n", old_size, new_size));

	evacuated_data = NULL;
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		if (l_ad + size_diff <= max_l_ad) {
			/* only reflect size change directly in the node */
			inflen  += size_diff;
			objsize += size_diff;
			l_ad    += size_diff;
			crclen = dscr_size - UDF_DESC_TAG_LENGTH + l_ea + l_ad;
			if (fe) {
				fe->inf_len   = udf_rw64(inflen);
				fe->l_ad      = udf_rw32(l_ad);
				fe->tag.desc_crc_len = udf_rw32(crclen);
			} else {
				efe->inf_len  = udf_rw64(inflen);
				efe->obj_size = udf_rw64(objsize);
				efe->l_ad     = udf_rw32(l_ad);
				efe->tag.desc_crc_len = udf_rw32(crclen);
			}
			error = 0;

			/* set new size for uvm */
			uvm_vnp_setsize(vp, old_size);
			uvm_vnp_setwritesize(vp, new_size);

#if 0
			/* zero append space in buffer */
			uvm_vnp_zerorange(vp, old_size, new_size - old_size);
#endif
	
			/* unlock */
			UDF_UNLOCK_NODE(udf_node, 0);

			udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
			KASSERT(new_inflen == orig_inflen + size_diff);
			KASSERT(new_lbrec == orig_lbrec);
			KASSERT(new_lbrec == 0);
			return 0;
		}

		DPRINTF(ALLOC, ("\tCONVERT from internal\n"));

		if (old_size > 0) {
			/* allocate some space and copy in the stuff to keep */
			evacuated_data = malloc(lb_size, M_UDFTEMP, M_WAITOK);
			memset(evacuated_data, 0, lb_size);

			/* node is locked, so safe to exit mutex */
			UDF_UNLOCK_NODE(udf_node, 0);

			/* read in using the `normal' vn_rdwr() */
			error = vn_rdwr(UIO_READ, udf_node->vnode,
					evacuated_data, old_size, 0, 
					UIO_SYSSPACE, IO_ALTSEMANTICS | IO_NODELOCKED,
					FSCRED, NULL, NULL);

			/* enter again */
			UDF_LOCK_NODE(udf_node, 0);
		}

		/* convert to a normal alloc */
		/* XXX HOWTO selecting allocation method ? */
		icbflags &= ~UDF_ICB_TAG_FLAGS_ALLOC_MASK;
		icbflags |=  UDF_ICB_LONG_ALLOC;	/* XXX or SHORT_ALLOC */
		icbtag->flags = udf_rw16(icbflags);

		/* wipe old descriptor space */
		udf_wipe_adslots(udf_node);

		memset(&c_ad, 0, sizeof(struct long_ad));
		c_ad.len          = udf_rw32(old_size | UDF_EXT_FREE);
		c_ad.loc.part_num = udf_rw16(0); /* not relevant */
		c_ad.loc.lb_num   = udf_rw32(0); /* not relevant */

		slot = 0;
	} else {
		/* goto the last entry (if any) */
		slot     = 0;
		cpy_slot = 0;
		foffset  = 0;
		memset(&c_ad, 0, sizeof(struct long_ad));
		for (;;) {
			udf_get_adslot(udf_node, slot, &c_ad, &eof);
			if (eof)
				break;

			len   = udf_rw32(c_ad.len);
			flags = UDF_EXT_FLAGS(len);
			len   = UDF_EXT_LEN(len);

			end_foffset = foffset + len;
			if (flags != UDF_EXT_REDIRECT)
				foffset = end_foffset;

			slot++;
		}
		/* at end of adslots */

		/* special case if the old size was zero, then there is no last slot */
		if (old_size == 0) {
			c_ad.len          = udf_rw32(0 | UDF_EXT_FREE);
			c_ad.loc.part_num = udf_rw16(0); /* not relevant */
			c_ad.loc.lb_num   = udf_rw32(0); /* not relevant */
		} else {
			/* refetch last slot */
			slot--;
			udf_get_adslot(udf_node, slot, &c_ad, &eof);
		}
	}

	/*
	 * If the length of the last slot is not a multiple of lb_size, adjust
	 * length so that it is; don't forget to adjust `append_len'! relevant for
	 * extending existing files
	 */
	len   = udf_rw32(c_ad.len);
	flags = UDF_EXT_FLAGS(len);
	len   = UDF_EXT_LEN(len);

	lastblock_grow = 0;
	if (len % lb_size > 0) {
		lastblock_grow = lb_size - (len % lb_size);
		lastblock_grow = MIN(size_diff, lastblock_grow);
		len += lastblock_grow;
		c_ad.len = udf_rw32(len | flags);

		/* TODO zero appened space in buffer! */
		/* using uvm_vnp_zerorange(vp, old_size, new_size - old_size); ? */
	}
	memset(&s_ad, 0, sizeof(struct long_ad));

	/* size_diff can be bigger than allowed, so grow in chunks */
	append_len = size_diff - lastblock_grow;
	while (append_len > 0) {
		chunk = MIN(append_len, max_len);
		s_ad.len = udf_rw32(chunk | UDF_EXT_FREE);
		s_ad.loc.part_num = udf_rw16(0);
		s_ad.loc.lb_num   = udf_rw32(0);

		if (udf_ads_merge(lb_size, &c_ad, &s_ad)) {
			/* not mergable (anymore) */
			error = udf_append_adslot(udf_node, slot, &c_ad);
			if (error)
				goto errorout;
			slot++;
			c_ad = s_ad;
			memset(&s_ad, 0, sizeof(struct long_ad));
		}
		append_len -= chunk;
	}

	/* if there is a rest piece in the accumulator, append it */
	if (UDF_EXT_LEN(c_ad.len) > 0) {
		error = udf_append_adslot(udf_node, slot, &c_ad);
		if (error)
			goto errorout;
		slot++;
	}

	/* if there is a rest piece that didn't fit, append it */
	if (UDF_EXT_LEN(s_ad.len) > 0) {
		error = udf_append_adslot(udf_node, slot, &s_ad);
		if (error)
			goto errorout;
		slot++;
	}

	inflen  += size_diff;
	objsize += size_diff;
	if (fe) {
		fe->inf_len   = udf_rw64(inflen);
	} else {
		efe->inf_len  = udf_rw64(inflen);
		efe->obj_size = udf_rw64(objsize);
	}
	error = 0;

	if (evacuated_data) {
		/* set new write size for uvm */
		uvm_vnp_setwritesize(vp, old_size);

		/* write out evacuated data */
		error = vn_rdwr(UIO_WRITE, udf_node->vnode,
				evacuated_data, old_size, 0, 
				UIO_SYSSPACE, IO_ALTSEMANTICS | IO_NODELOCKED,
				FSCRED, NULL, NULL);
		uvm_vnp_setsize(vp, old_size);
	}

errorout:
	if (evacuated_data)
		free(evacuated_data, M_UDFTEMP);
	UDF_UNLOCK_NODE(udf_node, 0);

	udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
	KASSERT(new_inflen == orig_inflen + size_diff);
	KASSERT(new_lbrec == orig_lbrec);

	return error;
}

/* --------------------------------------------------------------------- */

int
udf_shrink_node(struct udf_node *udf_node, uint64_t new_size)
{
	struct vnode *vp = udf_node->vnode;
	struct udf_mount *ump = udf_node->ump;
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct icb_tag  *icbtag;
	struct long_ad c_ad, s_ad, *node_ad_cpy;
	uint64_t size_diff, old_size, inflen, objsize;
	uint64_t foffset, end_foffset;
	uint64_t orig_inflen, orig_lbrec, new_inflen, new_lbrec;
	uint32_t lb_size, dscr_size, crclen;
	uint32_t slot_offset;
	uint32_t len, flags, max_len;
	uint32_t num_lb, lb_num;
	uint32_t max_l_ad, l_ad, l_ea;
	uint16_t vpart_num;
	uint8_t *data_pos;
	int icbflags, addr_type;
	int slot, cpy_slot, cpy_slots;
	int eof, error;

	DPRINTF(ALLOC, ("udf_shrink_node\n"));
	udf_node_sanity_check(udf_node, &orig_inflen, &orig_lbrec);

	UDF_LOCK_NODE(udf_node, 0);
	lb_size = udf_rw32(ump->logical_vol->lb_size);
	max_len = ((UDF_EXT_MAXLEN / lb_size) * lb_size);

	/* do the work */
	fe  = udf_node->fe;
	efe = udf_node->efe;
	if (fe) {
		icbtag  = &fe->icbtag;
		inflen  = udf_rw64(fe->inf_len);
		objsize = inflen;
		dscr_size  = sizeof(struct file_entry) -1;
		l_ea       = udf_rw32(fe->l_ea);
		l_ad       = udf_rw32(fe->l_ad);
		data_pos = (uint8_t *) fe + dscr_size + l_ea;
	} else {
		icbtag  = &efe->icbtag;
		inflen  = udf_rw64(efe->inf_len);
		objsize = udf_rw64(efe->obj_size);
		dscr_size  = sizeof(struct extfile_entry) -1;
		l_ea       = udf_rw32(efe->l_ea);
		l_ad       = udf_rw32(efe->l_ad);
		data_pos = (uint8_t *) efe + dscr_size + l_ea;
	}
	max_l_ad = lb_size - dscr_size - l_ea;

	icbflags   = udf_rw16(icbtag->flags);
	addr_type  = icbflags & UDF_ICB_TAG_FLAGS_ALLOC_MASK;

	old_size  = inflen;
	size_diff = old_size - new_size;

	DPRINTF(ALLOC, ("\tfrom %"PRIu64" to %"PRIu64"\n", old_size, new_size));

	/* shrink the node to its new size */
	if (addr_type == UDF_ICB_INTERN_ALLOC) {
		/* only reflect size change directly in the node */
		KASSERT(new_size <= max_l_ad);
		inflen  -= size_diff;
		objsize -= size_diff;
		l_ad    -= size_diff;
		crclen = dscr_size - UDF_DESC_TAG_LENGTH + l_ea + l_ad;
		if (fe) {
			fe->inf_len   = udf_rw64(inflen);
			fe->l_ad      = udf_rw32(l_ad);
			fe->tag.desc_crc_len = udf_rw32(crclen);
		} else {
			efe->inf_len  = udf_rw64(inflen);
			efe->obj_size = udf_rw64(objsize);
			efe->l_ad     = udf_rw32(l_ad);
			efe->tag.desc_crc_len = udf_rw32(crclen);
		}
		error = 0;
		/* TODO zero appened space in buffer! */
		/* using uvm_vnp_zerorange(vp, old_size, old_size - new_size); ? */

		/* set new size for uvm */
		uvm_vnp_setsize(vp, new_size);
		UDF_UNLOCK_NODE(udf_node, 0);

		udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
		KASSERT(new_inflen == orig_inflen - size_diff);
		KASSERT(new_lbrec == orig_lbrec);
		KASSERT(new_lbrec == 0);

		return 0;
	}

	/* setup node cleanup extents copy space */
	node_ad_cpy = malloc(lb_size * UDF_MAX_ALLOC_EXTENTS,
		M_UDFMNT, M_WAITOK);
	memset(node_ad_cpy, 0, lb_size * UDF_MAX_ALLOC_EXTENTS);

	/*
	 * Shrink the node by releasing the allocations and truncate the last
	 * allocation to the new size. If the new size fits into the
	 * allocation descriptor itself, transform it into an
	 * UDF_ICB_INTERN_ALLOC.
	 */
	slot     = 0;
	cpy_slot = 0;
	foffset  = 0;

	/* 1) copy till first overlap piece to the rewrite buffer */
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof) {
			DPRINTF(WRITE,
				("Shrink node failed: "
				 "encountered EOF\n"));
			error = EINVAL;
			goto errorout; /* panic? */
		}
		len   = udf_rw32(s_ad.len);
		flags = UDF_EXT_FLAGS(len);
		len   = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		end_foffset = foffset + len;
		if (end_foffset > new_size)
			break;	/* found */

		node_ad_cpy[cpy_slot++] = s_ad;

		DPRINTF(ALLOC, ("\t1: vp %d, lb %d, len %d, flags %d "
			"-> stack\n",
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		foffset = end_foffset;
		slot++;
	}
	slot_offset = new_size - foffset;

	/* 2) trunc overlapping slot at overlap and copy it */
	if (slot_offset > 0) {
		lb_num    = udf_rw32(s_ad.loc.lb_num);
		vpart_num = udf_rw16(s_ad.loc.part_num);

		if (flags == UDF_EXT_ALLOCATED) {
			lb_num += (slot_offset + lb_size -1) / lb_size;
			num_lb  = (len - slot_offset + lb_size - 1) / lb_size;

			udf_free_allocated_space(ump, lb_num, vpart_num, num_lb);
		}

		s_ad.len = udf_rw32(slot_offset | flags);
		node_ad_cpy[cpy_slot++] = s_ad;
		slot++;

		DPRINTF(ALLOC, ("\t2: vp %d, lb %d, len %d, flags %d "
			"-> stack\n",
			udf_rw16(s_ad.loc.part_num),
			udf_rw32(s_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(s_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));
	}

	/* 3) delete remainder */
	for (;;) {
		udf_get_adslot(udf_node, slot, &s_ad, &eof);
		if (eof)
			break;

		len       = udf_rw32(s_ad.len);
		flags     = UDF_EXT_FLAGS(len);
		len       = UDF_EXT_LEN(len);

		if (flags == UDF_EXT_REDIRECT) {
			slot++;
			continue;
		}

		DPRINTF(ALLOC, ("\t3: delete remainder "
			"vp %d lb %d, len %d, flags %d\n",
		udf_rw16(s_ad.loc.part_num),
		udf_rw32(s_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(s_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		if (flags == UDF_EXT_ALLOCATED) {
			lb_num    = udf_rw32(s_ad.loc.lb_num);
			vpart_num = udf_rw16(s_ad.loc.part_num);
			num_lb    = (len + lb_size - 1) / lb_size;

			udf_free_allocated_space(ump, lb_num, vpart_num,
				num_lb);
		}

		slot++;
	}

	/* 4) if it will fit into the descriptor then convert */
	if (new_size < max_l_ad) {
		/*
		 * resque/evacuate old piece by reading it in, and convert it
		 * to internal alloc.
		 */
		if (new_size == 0) {
			/* XXX/TODO only for zero sizing now */
			udf_wipe_adslots(udf_node);

			icbflags &= ~UDF_ICB_TAG_FLAGS_ALLOC_MASK;
			icbflags |=  UDF_ICB_INTERN_ALLOC;
			icbtag->flags = udf_rw16(icbflags);

			inflen  -= size_diff;	KASSERT(inflen == 0);
			objsize -= size_diff;
			l_ad     = new_size;
			crclen = dscr_size - UDF_DESC_TAG_LENGTH + l_ea + l_ad;
			if (fe) {
				fe->inf_len   = udf_rw64(inflen);
				fe->l_ad      = udf_rw32(l_ad);
				fe->tag.desc_crc_len = udf_rw32(crclen);
			} else {
				efe->inf_len  = udf_rw64(inflen);
				efe->obj_size = udf_rw64(objsize);
				efe->l_ad     = udf_rw32(l_ad);
				efe->tag.desc_crc_len = udf_rw32(crclen);
			}
			/* eventually copy in evacuated piece */
			/* set new size for uvm */
			uvm_vnp_setsize(vp, new_size);

			free(node_ad_cpy, M_UDFMNT);
			UDF_UNLOCK_NODE(udf_node, 0);

			udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
			KASSERT(new_inflen == orig_inflen - size_diff);
			KASSERT(new_inflen == 0);
			KASSERT(new_lbrec == 0);

			return 0;
		}

		printf("UDF_SHRINK_NODE: could convert to internal alloc!\n");
	}

	/* 5) reset node descriptors */
	udf_wipe_adslots(udf_node);

	/* 6) copy back extents; merge when possible. Recounting on the fly */
	cpy_slots = cpy_slot;

	c_ad = node_ad_cpy[0];
	slot = 0;
	for (cpy_slot = 1; cpy_slot < cpy_slots; cpy_slot++) {
		s_ad = node_ad_cpy[cpy_slot];

		DPRINTF(ALLOC, ("\t6: stack -> got mapping vp %d "
			"lb %d, len %d, flags %d\n",
		udf_rw16(s_ad.loc.part_num),
		udf_rw32(s_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(s_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(s_ad.len)) >> 30));

		/* see if we can merge */
		if (udf_ads_merge(lb_size, &c_ad, &s_ad)) {
			/* not mergable (anymore) */
			DPRINTF(ALLOC, ("\t6: appending vp %d lb %d, "
				"len %d, flags %d\n",
			udf_rw16(c_ad.loc.part_num),
			udf_rw32(c_ad.loc.lb_num),
			UDF_EXT_LEN(udf_rw32(c_ad.len)),
			UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

			error = udf_append_adslot(udf_node, slot, &c_ad);
			if (error)
				goto errorout; /* panic? */
			c_ad = s_ad;
			slot++;
		}
	}

	/* 7) push rest slot (if any) */
	if (UDF_EXT_LEN(c_ad.len) > 0) {
		DPRINTF(ALLOC, ("\t7: last append vp %d lb %d, "
				"len %d, flags %d\n",
		udf_rw16(c_ad.loc.part_num),
		udf_rw32(c_ad.loc.lb_num),
		UDF_EXT_LEN(udf_rw32(c_ad.len)),
		UDF_EXT_FLAGS(udf_rw32(c_ad.len)) >> 30));

		error = udf_append_adslot(udf_node, slot, &c_ad);
		if (error)
			goto errorout; /* panic? */
		;
	}

	inflen  -= size_diff;
	objsize -= size_diff;
	if (fe) {
		fe->inf_len   = udf_rw64(inflen);
	} else {
		efe->inf_len  = udf_rw64(inflen);
		efe->obj_size = udf_rw64(objsize);
	}
	error = 0;

	/* set new size for uvm */
	uvm_vnp_setsize(vp, new_size);

errorout:
	free(node_ad_cpy, M_UDFMNT);
	UDF_UNLOCK_NODE(udf_node, 0);

	udf_node_sanity_check(udf_node, &new_inflen, &new_lbrec);
	KASSERT(new_inflen == orig_inflen - size_diff);
	KASSERT(new_lbrec == orig_lbrec);

	return error;
}


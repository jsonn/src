/*	$NetBSD: disklabel_acorn.h,v 1.1.6.2 2000/11/20 20:02:40 bouyer Exp $	*/

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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
 * disklabel.h
 *
 * machine specific disk label info
 *
 * Created      : 04/10/94
 */

#define NRISCBSD_PARTITIONS MAXPARTITIONS

#define PARTITION_TYPE_UNUSED  0
#define PARTITION_TYPE_ADFS    1
#define PARTITION_TYPE_RISCIX  2

#define PARTITION_FORMAT_RISCIX  2
#define PARTITION_FORMAT_RISCBSD 0x42

#define FILECORE_BOOT_SECTOR 6

/* Stuff to deal with RISCiX partitions */

#define NRISCIX_PARTITIONS 8
#define RISCIX_PARTITION_OFFSET 8

struct riscix_partition {
	u_int rp_start;
	u_int rp_length;
	u_int rp_type;
	char rp_name[16];
};

struct riscix_partition_table {
	u_int pad0;
	u_int pad1;
	struct riscix_partition partitions[NRISCIX_PARTITIONS];
};

struct riscbsd_partition {
	u_int rp_start;
	u_int rp_length;
	u_int rp_type;
	char rp_name[16];
};

struct filecore_bootblock {
	u_char  padding0[0x1c0];
	u_char  log2secsize;
	u_char  secspertrack;
	u_char  heads;
	u_char  density;
	u_char  idlen;
	u_char  log2bpmb;
	u_char  skew;
	u_char  bootoption;
	u_char  lowsector;
	u_char  nzones;
	u_short zone_spare;
	u_int   root;
	u_int   disc_size;
	u_short disc_id;
	u_char  disc_name[10];
	u_int   disc_type;

	u_char  padding1[24];

	u_char partition_type;
	u_char partition_cyl_low;
	u_char partition_cyl_high;
	u_char checksum;
};

#if defined(_KERNEL) && !defined(__ASSEMBLER__)
struct buf;
struct cpu_disklabel;
struct disklabel;

/* for readdisklabel.  rv != 0 -> matches, msg == NULL -> success */
int	filecore_label_read __P((dev_t, void (*)(struct buf *),
	    struct disklabel *, struct cpu_disklabel *, char **, int *,
	    int *));
/* for writedisklabel.  rv == 0 -> dosen't match, rv > 0 -> success */
int	filecore_label_locate __P((dev_t, void (*)(struct buf *),
	    struct disklabel *, struct cpu_disklabel *, int *, int *));
#endif

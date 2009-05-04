/*	$NetBSD: privahdi.h,v 1.1.1.1.134.2 2009/05/04 08:10:48 yamt Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman, Waldi Ravens and Leo Weppelman.
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

#include <sys/param.h>
#include <sys/disklabel.h>
#include <machine/ahdilabel.h>

/* Flags for ahdi_readlabel() */
#define	FORCE_AHDI	0x01	/* Read AHDI label when NetBSD label exists */
#define	AHDI_IGN_EXISTS	0x02	/* Ignore partition exists flag (ICD tools) */
#define	AHDI_IGN_EXT	0x04	/* Ignore last extended parition (HDDriver) */
#define	AHDI_IGN_CKSUM	0x08	/* Ignore checksum mismatch on root sector */
#define	AHDI_IGN_SPU	0x10	/* Ignore total sectors mismatch */

/* Flags for ahdi_writelabel() */
#define AHDI_KEEP_BOOT	0x01	/* Keep boot sector */
#define AHDI_KEEP_BSL	0x02	/* Keep bad sector list */
#define AHDI_KEEP_NBDA	0x04	/* Keep NetBSD label */

struct ptable_part {
	u_int8_t	flag;	/* partition flag */
	u_int8_t	id[3];	/* id: GEM, BGM, NBD, ... */
	u_int32_t	root;	/* root sector */
	u_int32_t	start;	/* start sector */
	u_int32_t	size;	/* size in sectors */
	int		letter;	/* partition letter */
};

struct ahdi_ptable {
	u_int32_t		nsectors;	/* number of sectors/track */
	u_int32_t		ntracks;	/* number of tracks/cylinder */
	u_int32_t		ncylinders;	/* number of cylinders */
	u_int32_t		secpercyl;	/* number of sectors/cylinder */
	u_int32_t		secperunit;	/* number of total sectors */
	int			nparts;		/* number of partitions */
	struct ptable_part	parts[MAXPARTITIONS];
};

int	 ahdi_buildlabel(struct ahdi_ptable *);
int	 ahdi_checklabel(struct ahdi_ptable *);
int	 ahdi_readlabel(struct ahdi_ptable *, char *, int);
int	 ahdi_writedisktab(struct ahdi_ptable *, char *, char *, char *);
int	 ahdi_writelabel(struct ahdi_ptable *, char *, int);

extern int	ahdi_errp1, ahdi_errp2;

/* Internal functions */
u_int16_t		 ahdi_cksum(void *);
void			 assign_letters(struct ahdi_ptable *);
int			 check_magic(int, u_int, int);
int			 dkcksum(struct disklabel *);
void			*disk_read(int, u_int, u_int);
int			 disk_write(int, u_int, u_int, void *);
int			 invalidate_netbsd_label(int, u_int32_t);
int			 openraw(const char *, int);
struct disklabel	*read_dl(int);
int			 read_rsec(int, struct ahdi_ptable *, u_int,
			     u_int, int);
int			 write_bsl(int);

/*
 * Copyright (c) 1994 Scott Bartram
 * All rights reserved.
 *
 * adapted from sys/sys/exec_ecoff.h
 * based on Intel iBCS2
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 *
 */

#ifndef	_IBCS2_EXEC_H_
#define	_IBCS2_EXEC_H_

/*
 * COFF file header
 */

struct coff_filehdr {
    u_short	f_magic;	/* magic number */
    u_short	f_nscns;	/* # of sections */
    long	f_timdat;	/* timestamp */
    long	f_symptr;	/* file offset of symbol table */
    long	f_nsyms;	/* # of symbol table entries */
    u_short	f_opthdr;	/* size of optional header */
    u_short	f_flags;	/* flags */
};

/* f_magic flags */
#define COFF_MAGIC_I386	0x14c

/* f_flags */
#define COFF_F_RELFLG	0x1
#define COFF_F_EXEC	0x2
#define COFF_F_LNNO	0x4
#define COFF_F_LSYMS	0x8
#define COFF_F_SWABD	0x40
#define COFF_F_AR16WR	0x80
#define COFF_F_AR32WR	0x100

/*
 * COFF system header
 */

struct coff_aouthdr {
    short	a_magic;
    short	a_vstamp;
    long	a_tsize;
    long	a_dsize;
    long	a_bsize;
    long	a_entry;
    long	a_tstart;
    long	a_dstart;
};

/* magic */
#define COFF_OMAGIC	0407	/* text not write-protected; data seg
				   is contiguous with text */
#define COFF_NMAGIC	0410	/* text is write-protected; data starts
				   at next seg following text */
#define COFF_ZMAGIC	0413	/* text and data segs are aligned for
				   direct paging */
#define COFF_SMAGIC	0443	/* shared lib */

/*
 * COFF section header
 */

struct coff_scnhdr {
    char	s_name[8];
    long	s_paddr;
    long	s_vaddr;
    long	s_size;
    long	s_scnptr;
    long	s_relptr;
    long	s_lnnoptr;
    u_short	s_nreloc;
    u_short	s_nlnno;
    long	s_flags;
};

/* s_flags */
#define COFF_STYP_REG		0x00
#define COFF_STYP_DSECT		0x01
#define COFF_STYP_NOLOAD	0x02
#define COFF_STYP_GROUP		0x04
#define COFF_STYP_PAD		0x08
#define COFF_STYP_COPY		0x10
#define COFF_STYP_TEXT		0x20
#define COFF_STYP_DATA		0x40
#define COFF_STYP_BSS		0x80
#define COFF_STYP_INFO		0x200
#define COFF_STYP_OVER		0x400
#define COFF_STYP_SHLIB		0x800

/*
 * COFF shared library header
 */

struct coff_slhdr {
	long	entry_len;	/* in words */
	long	path_index;	/* in words */
	char	sl_name[1];
};

#define COFF_ROUND(val, by)     (((val) + by - 1) & ~(by - 1))

#define COFF_ALIGN(a) ((a) & ~(COFF_LDPGSZ - 1))

#define COFF_HDR_SIZE \
	(sizeof(struct coff_filehdr) + sizeof(struct coff_aouthdr))

#define COFF_BLOCK_ALIGN(ap, value) \
        (ap->a_magic == COFF_ZMAGIC ? COFF_ROUND(value, COFF_LDPGSZ) : \
         value)

#define COFF_TXTOFF(fp, ap) \
        (ap->a_magic == COFF_ZMAGIC ? 0 : \
         COFF_ROUND(COFF_HDR_SIZE + fp->f_nscns * \
		    sizeof(struct coff_scnhdr), COFF_SEGMENT_ALIGNMENT(ap)))

#define COFF_DATOFF(fp, ap) \
        (COFF_BLOCK_ALIGN(ap, COFF_TXTOFF(fp, ap) + ap->a_tsize))

#define COFF_SEGMENT_ALIGN(ap, value) \
        (COFF_ROUND(value, (ap->a_magic == COFF_ZMAGIC ? COFF_LDPGSZ : \
         COFF_SEGMENT_ALIGNMENT(ap))))

#define COFF_LDPGSZ 4096

#define COFF_SEGMENT_ALIGNMENT(ap) 4

#define COFF_BADMAG(ex) (ex->f_magic != COFF_MAGIC_I386)

#define IBCS2_HIGH_SYSCALL(n)		(((n) & 0x7f) == 0x28)
#define IBCS2_CVT_HIGH_SYSCALL(n)	(((n) >> 8) + 128)

int     exec_ibcs2_coff_makecmds __P((struct proc *, struct exec_package *));

/*
 * x.out (XENIX)
 */

struct xexec {
	u_short	x_magic;	/* magic number */
	u_short	x_ext;		/* size of extended header */
	long	x_text;		/* ignored */
	long	x_data;		/* ignored */
	long	x_bss;		/* ignored */
	long	x_syms;		/* ignored */
	long	x_reloc;	/* ignored */
	long	x_entry;	/* executable entry point */
	char	x_cpu;		/* processor type */
	char	x_relsym;	/* ignored */
	u_short	x_renv;		/* flags */
};

/* x_magic flags */
#define XOUT_MAGIC	0x0206

/* x_cpu flags */
#define XC_386		0x004a	/* 386, word-swapped */

/* x_renv flags */
#define XE_V5		0xc000
#define XE_SEG		0x0800
#define XE_ABS		0x0400
#define XE_ITER		0x0200
#define XE_VMOD		0x0100
#define XE_FPH		0x0080
#define XE_LTEXT	0x0040
#define XE_LDATA	0x0020
#define XE_OVER		0x0010
#define XE_FS		0x0008
#define XE_PURE		0x0004
#define XE_SEP		0x0002
#define XE_EXEC		0x0001

/*
 * x.out extended header
 */

struct xext {
	long	xe_trsize;	/* ignored */
	long	xe_drsize;	/* ignored */
	long	xe_tbase;	/* ignored */
	long	xe_dbase;	/* ignored */
	long	xe_stksize;	/* stack size if XE_FS set in x_renv */
	long	xe_segpos;	/* offset of segment table */
	long	xe_segsize;	/* segment table size */
	long	xe_mdtpos;	/* ignored */
	long	xe_mdtsize;	/* ignored */
	char	xe_mdttype;	/* ignored */
	char	xe_pagesize;	/* ignored */
	char	xe_ostype;	/* ignored */
	char	xe_osvers;	/* ignored */
	u_short	xe_eseg;	/* ignored */
	u_short	xe_sres;	/* ignored */
};

/*
 * x.out segment table
 */

struct xseg {
	u_short	xs_type;	/* segment type */
	u_short	xs_attr;	/* attribute flags */
	u_short	xs_seg;		/* segment selector number */
	char	xs_align;	/* ignored */
	char	xs_cres;	/* ignored */
	long	xs_filpos;	/* offset of this segment */
	long	xs_psize;	/* physical segment size */
	long	xs_vsize;	/* virtual segment size */
	long	xs_rbase;	/* relocation base address */
	u_short	xs_noff;	/* ignored */
	u_short	xs_sres;	/* ignored */
	long	xs_lres;	/* ignored */
};

/* xs_type flags */
#define	XS_TNULL	0	/* unused */
#define	XS_TTEXT	1	/* text (read-only) */
#define	XS_TDATA	2	/* data (read-write) */
#define	XS_TSYMS	3	/* symbol table (noload) */
#define	XS_TREL		4	/* relocation segment (noload) */
#define	XS_TSESTR	5	/* string table (noload) */
#define	XS_TGRPS	6	/* group segment (noload) */

#define	XS_TIDATA	64
#define	XS_TTSS		65
#define	XS_TLFIX	66
#define	XS_TDNAME	67
#define	XS_TDTEXT	68
#define	XS_TDFIX	69
#define	XS_TOVTAB	70
#define	XS_T71		71
#define	XS_TSYSTR	72

/* xs_attr flags */
#define XS_AMEM		0x8000	/* memory image */
#define XS_AITER	0x0001	/* iteration records */
#define XS_AHUGE	0x0002	/* unused */
#define XS_ABSS		0x0004	/* uninitialized data */
#define XS_APURE	0x0008	/* read-only (sharable) segment */
#define XS_AEDOWN	0x0010	/* expand down memory segment */
#define XS_APRIV	0x0020	/* unused */
#define	XS_A32BIT	0x0040	/* 32-bit text/data */

/*
 * x.out iteration record
 */

struct xiter {
	long	xi_size;	/* text/data size */
	long	xi_rep;		/* number of replications */
	long	xi_offset;	/* offset within segment to replicated data */
};

#define XOUT_HDR_SIZE		(sizeof(struct xexec) + sizeof(struct xext))

int     exec_ibcs2_xout_makecmds __P((struct proc *, struct exec_package *));

#endif /* !_IBCS2_EXEC_H_ */

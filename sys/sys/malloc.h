/*	$NetBSD: malloc.h,v 1.64.2.6 2002/09/06 08:49:57 jdolecek Exp $	*/

/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)malloc.h	8.5 (Berkeley) 5/3/95
 */

#ifndef _SYS_MALLOC_H_
#define	_SYS_MALLOC_H_

#if defined(_KERNEL_OPT)
#include "opt_kmemstats.h"
#include "opt_malloclog.h"
#include "opt_malloc_debug.h"
#include "opt_lockdebug.h"
#endif

/*
 * flags to malloc
 */
#define	M_WAITOK	0x0000	/* can wait for resources */
#define	M_NOWAIT	0x0001	/* do not wait for resources */
#define	M_ZERO		0x0002	/* zero the allocation */
#define	M_CANFAIL	0x0004	/* can fail if requested memory can't ever
				 * be allocated */
/*
 * Types of memory to be allocated (don't forget to update malloc.9)
 */
#define	M_FREE		0	/* should be on free list */
#define	M_MBUF		1	/* mbuf */
#define	M_DEVBUF	2	/* device driver memory */
#define	M_SOCKET	3	/* socket structure */
#define	M_PCB		4	/* protocol control block */
#define	M_RTABLE	5	/* routing tables */
#define	M_HTABLE	6	/* IMP host tables */
#define	M_FTABLE	7	/* fragment reassembly header */
#define	M_ZOMBIE	8	/* zombie proc status */
#define	M_IFADDR	9	/* interface address */
#define	M_SOOPTS	10	/* socket options */
#define	M_SONAME	11	/* socket name */
#define	M_NAMEI		12	/* namei path name buffer */
#define	M_GPROF		13	/* kernel profiling buffer */
#define	M_IOCTLOPS	14	/* ioctl data buffer */
#define	M_MAPMEM	15	/* mapped memory descriptors */
#define	M_CRED		16	/* credentials */
#define	M_PGRP		17	/* process group header */
#define	M_SESSION	18	/* session header */
#define	M_IOV		19	/* large iov's */
#define	M_MOUNT		20	/* vfs mount struct */
#define	M_FHANDLE	21	/* network file handle */
#define	M_NFSREQ	22	/* NFS request header */
#define	M_NFSMNT	23	/* NFS mount structure */
#define	M_NFSNODE	24	/* NFS vnode private part */
#define	M_VNODE		25	/* Dynamically allocated vnodes */
#define	M_CACHE		26	/* Dynamically allocated cache entries */
#define	M_DQUOT		27	/* UFS quota entries */
#define	M_UFSMNT	28	/* UFS mount structure */
#define	M_SHM		29	/* SVID compatible shared memory segments */
#define	M_VMMAP		30	/* VM map structures */
#define	M_VMMAPENT	31	/* VM map entry structures */
#define	M_VMOBJ		32	/* VM object structure */
#define	M_VMOBJHASH	33	/* VM object hash structure */
#define	M_VMPMAP	34	/* VM pmap */
#define	M_VMPVENT	35	/* VM phys-virt mapping entry */
#define	M_VMPAGER	36	/* XXX: VM pager struct */
#define	M_VMPGDATA	37	/* XXX: VM pager private data */
#define	M_FILE		38	/* Open file structure */
#define	M_FILEDESC	39	/* Open file descriptor table */
#define	M_LOCKF		40	/* Byte-range locking structures */
#define	M_PROC		41	/* Proc structures */
#define	M_SUBPROC	42	/* Proc sub-structures */
#define	M_SEGMENT	43	/* Segment for LFS */
#define	M_LFSNODE	44	/* LFS vnode private part */
#define	M_FFSNODE	45	/* FFS vnode private part */
#define	M_MFSNODE	46	/* MFS vnode private part */
#define	M_NQLEASE	47	/* Nqnfs lease */
#define	M_NQMHOST	48	/* Nqnfs host address table */
#define	M_NETADDR	49	/* Export host address structure */
#define	M_NFSSVC	50	/* Nfs server structure */
#define	M_NFSUID	51	/* Nfs uid mapping structure */
#define	M_NFSD		52	/* Nfs server daemon structure */
#define	M_IPMOPTS	53	/* internet multicast options */
#define	M_IPMADDR	54	/* internet multicast address */
#define	M_IFMADDR	55	/* link-level multicast address */
#define	M_MRTABLE	56	/* multicast routing tables */
#define	M_ISOFSMNT	57	/* ISOFS mount structure */
#define	M_ISOFSNODE	58	/* ISOFS vnode private part */
#define	M_MSDOSFSMNT	59	/* MSDOS FS mount structure */
#define	M_MSDOSFSFAT	60	/* MSDOS FS fat table */
#define	M_MSDOSFSNODE	61	/* MSDOS FS vnode private part */
#define	M_TTYS		62	/* allocated tty structures */
#define	M_EXEC		63	/* argument lists & other mem used by exec */
#define	M_MISCFSMNT	64	/* miscfs mount structures */
#define	M_MISCFSNODE	65	/* miscfs vnode private part */
#define	M_ADOSFSMNT	66	/* adosfs mount structures */
#define	M_ADOSFSNODE	67	/* adosfs vnode private part */
#define	M_ANODE		68	/* adosfs anode structures and tables */
#define	M_IPQ		69	/* IP packet queue entry */
#define	M_AFS		70	/* Andrew File System */
#define	M_ADOSFSBITMAP	71	/* adosfs bitmap */
#define	M_NFSRVDESC	72	/* NFS server descriptor */
#define	M_NFSDIROFF	73	/* NFS directory cookies */
#define	M_NFSBIGFH	74	/* NFS big filehandle */
#define	M_EXT2FSNODE	75	/* EXT2FS vnode private part */
#define	M_VMSWAP	76	/* VM swap structures */
#define	M_VMPAGE	77	/* VM page structures */
#define	M_VMPBUCKET	78	/* VM page buckets */
/*
 * Why are 79-81 empty?
 */
#define	M_UVMAMAP	82	/* UVM amap and related structs */
#define	M_UVMAOBJ	83	/* UVM aobj and related structs */
#define	M_TEMP		84	/* misc temporary data buffers */
#define	M_DMAMAP	85	/* bus_dma(9) structures */
#define	M_IPFLOW	86	/* IP flow entries */
#define	M_USB		87	/* USB general */
#define	M_USBDEV	88	/* USB device driver */
#define	M_POOL		89	/* memory pool structs */
#define	M_CODA		90	/* Coda file system structures and tables */
#define	M_FILECOREMNT	91	/* Filecore FS mount structures */
#define	M_FILECORENODE	92	/* Filecore FS vnode private part */
#define	M_RAIDFRAME	93	/* RAIDframe structures */
#define	M_USBHC		94	/* USB host controller */
#define	M_SECA		95	/* security associations, key management */
#define	M_IP6OPT	96	/* IPv6 options */
#define	M_IP6NDP	97	/* IPv6 Neighbour Discovery */
#define	M_NTFS		98	/* Windows NT file system structures */
#define	M_PAGEDEP	99	/* File page dependencies */
#define	M_INODEDEP	100	/* Inode dependencies */
#define	M_NEWBLK	101	/* New block allocation */
#define	M_BMSAFEMAP	102	/* Block or frag allocated from cyl group map */
#define	M_ALLOCDIRECT	103	/* Block or frag dependency for an inode */
#define	M_INDIRDEP	104	/* Indirect block dependencies */
#define	M_ALLOCINDIR	105	/* Block dependency for an indirect block */
#define	M_FREEFRAG	106	/* Previously used frag for an inode */
#define	M_FREEBLKS	107	/* Blocks freed from an inode */
#define	M_FREEFILE	108	/* Inode deallocated */
#define	M_DIRADD	109	/* New directory entry */
#define	M_MKDIR		110	/* New directory */
#define	M_DIRREM	111 	/* Directory entry deleted */
#define	M_IP6RR		112	/* IPv6 Router Renumbering Prefix */
#define	M_RR_ADDR	113	/* IPv6 Router Renumbering Ifid */
#define	M_SOFTINTR	114	/* Softinterrupt structures */
#define	M_EMULDATA	115	/* Per-process emulation data */
#define	M_1394CTL	116	/* IEEE 1394 control structures */
#define	M_1394DATA	117	/* IEEE 1394 data buffers */
#define	M_PIPE		118	/* Pipe structures */
#define	M_AGP		119	/* AGP memory */
#define	M_PROP		120	/* Kernel properties structures */
#define	M_NEWDIRBLK	121	/* Unclaimed new dir block (softdeps) */
#define	M_SMBIOD	122	/* SMB network id daemon */
#define	M_SMBCONN	123	/* SMB connection */
#define	M_SMBRQ		124	/* SMB request */
#define	M_SMBDATA	125	/* Misc netsmb data */
#define	M_SMBSTR	126	/* netsmb string data */
#define	M_SMBTEMP	127	/* Temp netsmb data */
#define	M_ICONV		128	/* ICONV data */
#define	M_SMBNODE	129	/* SMBFS node */
#define	M_SMBNODENAME	130	/* SMBFS node name */
#define	M_SMBFSDATA	131	/* SMBFS private data */
#define	M_SMBFSHASH	132	/* SMBFS hash table */
#define	M_SA		133	/* Scheduler activations */
#define	M_KEVENT	134	/* kevents/knotes */
#define	M_LAST		135	/* Must be last type + 1 */

/* added something?  don't forget to update malloc.9 */

#define	INITKMEMNAMES { \
	"free",		/* 0 M_FREE */ \
	"mbuf",		/* 1 M_MBUF */ \
	"devbuf",	/* 2 M_DEVBUF */ \
	"socket",	/* 3 M_SOCKET */ \
	"pcb",		/* 4 M_PCB */ \
	"routetbl",	/* 5 M_RTABLE */ \
	"hosttbl",	/* 6 M_HTABLE */ \
	"fragtbl",	/* 7 M_FTABLE */ \
	"zombie",	/* 8 M_ZOMBIE */ \
	"ifaddr",	/* 9 M_IFADDR */ \
	"soopts",	/* 10 M_SOOPTS */ \
	"soname",	/* 11 M_SONAME */ \
	"namei",	/* 12 M_NAMEI */ \
	"gprof",	/* 13 M_GPROF */ \
	"ioctlops",	/* 14 M_IOCTLOPS */ \
	"mapmem",	/* 15 M_MAPMEM */ \
	"cred",		/* 16 M_CRED */ \
	"pgrp",		/* 17 M_PGRP */ \
	"session",	/* 18 M_SESSION */ \
	"iov",		/* 19 M_IOV */ \
	"mount",	/* 20 M_MOUNT */ \
	"fhandle",	/* 21 M_FHANDLE */ \
	"NFS req",	/* 22 M_NFSREQ */ \
	"NFS mount",	/* 23 M_NFSMNT */ \
	"NFS node",	/* 24 M_NFSNODE */ \
	"vnodes",	/* 25 M_VNODE */ \
	"namecache",	/* 26 M_CACHE */ \
	"UFS quota",	/* 27 M_DQUOT */ \
	"UFS mount",	/* 28 M_UFSMNT */ \
	"shm",		/* 29 M_SHM */ \
	"VM map",	/* 30 M_VMMAP */ \
	"VM mapent",	/* 31 M_VMMAPENT */ \
	"VM object",	/* 32 M_VMOBJ */ \
	"VM objhash",	/* 33 M_VMOBJHASH */ \
	"VM pmap",	/* 34 M_VMPMAP */ \
	"VM pvmap",	/* 35 M_VMPVENT */ \
	"VM pager",	/* 36 M_VMPAGER */ \
	"VM pgdata",	/* 37 M_VMPGDATA */ \
	"file",		/* 38 M_FILE */ \
	"file desc",	/* 39 M_FILEDESC */ \
	"lockf",	/* 40 M_LOCKF */ \
	"proc",		/* 41 M_PROC */ \
	"subproc",	/* 42 M_SUBPROC */ \
	"LFS segment",	/* 43 M_SEGMENT */ \
	"LFS node",	/* 44 M_LFSNODE */ \
	"FFS node",	/* 45 M_FFSNODE */ \
	"MFS node",	/* 46 M_MFSNODE */ \
	"NQNFS Lease",	/* 47 M_NQLEASE */ \
	"NQNFS Host",	/* 48 M_NQMHOST */ \
	"Export Host",	/* 49 M_NETADDR */ \
	"NFS srvsock",	/* 50 M_NFSSVC */ \
	"NFS uid",	/* 51 M_NFSUID */ \
	"NFS daemon",	/* 52 M_NFSD */ \
	"ip_moptions",	/* 53 M_IPMOPTS */ \
	"in_multi",	/* 54 M_IPMADDR */ \
	"ether_multi",	/* 55 M_IFMADDR */ \
	"mrt",		/* 56 M_MRTABLE */ \
	"ISOFS mount",	/* 57 M_ISOFSMNT */ \
	"ISOFS node",	/* 58 M_ISOFSNODE */ \
	"MSDOSFS mount", /* 59 M_MSDOSFSMNT */ \
	"MSDOSFS fat",	/* 60 M_MSDOSFSFAT */ \
	"MSDOSFS node",	/* 61 M_MSDOSFSNODE */ \
	"ttys",		/* 62 M_TTYS */ \
	"exec",		/* 63 M_EXEC */ \
	"miscfs mount",	/* 64 M_MISCFSMNT */ \
	"miscfs node",	/* 65 M_MISCFSNODE */ \
	"adosfs mount",	/* 66 M_ADOSFSMNT */ \
	"adosfs node",	/* 67 M_ADOSFSNODE */ \
	"adosfs anode",	/* 68 M_ANODE */ \
	"IP queue ent", /* 69 M_IPQ */ \
	"afs",		/* 70 M_AFS */ \
	"adosfs bitmap", /* 71 M_ADOSFSBITMAP */ \
	"NFS srvdesc",	/* 72 M_NFSRVDESC */ \
	"NFS diroff",	/* 73 M_NFSDIROFF */ \
	"NFS bigfh",	/* 74 M_NFSBIGFH */ \
	"EXT2FS node",  /* 75 M_EXT2FSNODE */ \
	"VM swap",	/* 76 M_VMSWAP */ \
	"VM page",	/* 77 M_VMPAGE */ \
	"VM page bucket", /* 78 M_VMPBUCKET */ \
	NULL,		/* 79 */ \
	NULL,		/* 80 */ \
	NULL,		/* 81 */ \
	"UVM amap",	/* 82 M_UVMAMAP */ \
	"UVM aobj",	/* 83 M_UVMAOBJ */ \
	"temp",		/* 84 M_TEMP */ \
	"DMA map",	/* 85 M_DMAMAP */ \
	"IP flow",	/* 86 M_IPFLOW */ \
	"USB",		/* 87 M_USB */ \
	"USB device",	/* 88 M_USBDEV */ \
	"Pool",		/* 89 M_POOL */ \
	"coda",		/* 90 M_CODA */ \
	"filecore mount", /* 91 M_FILECOREMNT */ \
	"filecore node", /* 92 M_FILECORENODE */ \
	"RAIDframe",	/* 93 M_RAIDFRAME */ \
	"USB HC",	/* 94 M_USBHC */ \
	"key mgmt",	/* 95 M_SECA */ \
	"ip6_options",	/* 96 M_IP6OPT */ \
	"NDP",		/* 97 M_IP6NDP */ \
	"NTFS",		/* 98 M_NTFS */ \
	"pagedep",	/* 99 M_PAGEDEP */ \
	"inodedep",	/* 100 M_INODEDEP */ \
	"newblk",	/* 101 M_NEWBLK */ \
	"bmsafemap",	/* 102 M_BMSAFEMAP */ \
	"allocdirect",	/* 103 M_ALLOCDIRECT */ \
	"indirdep",	/* 104 M_INDIRDEP */ \
	"allocindir",	/* 105 M_ALLOCINDIR */ \
	"freefrag",	/* 106 M_FREEFRAG */ \
	"freeblks",	/* 107 M_FREEBLKS */ \
	"freefile",	/* 108 M_FREEFILE */ \
	"diradd",	/* 109 M_DIRADD */ \
	"mkdir",	/* 110 M_MKDIR */ \
	"dirrem",	/* 111 M_DIRREM */ \
	"ip6rr",	/* 112 M_IP6RR */ \
	"rp_addr",	/* 113 M_RR_ADDR */ \
	"softintr",	/* 114 M_SOFTINTR */ \
	"emuldata",	/* 115 M_EMULDATA */ \
	"1394ctl",	/* 116 M_1394CTL */ \
	"1394data",	/* 117 M_1394DATA */ \
	"pipe",		/* 118 M_PIPE */ \
	"AGP",		/* 119 M_AGP */ \
	"prop",		/* 120 M_PROP */ \
	"newdirblk",	/* 121 M_NEWDIRBLK */ \
	"smbiod",	/* 122 M_SMBIOD */ \
	"smbconn",	/* 123 M_SMBCONN */ \
	"smbrq",	/* 124 M_SMBRQ */ \
	"smbdata",	/* 125 M_SMBDATA */ \
	"smbstr",	/* 126 M_SMBDATA */ \
	"smbtemp",	/* 127 M_SMBTEMP */ \
	"iconv",	/* 128 M_ICONV */ \
	"smbnode",	/* 129 M_SMBNODE */ \
	"smbnodename",	/* 130 M_SMBNODENAME */ \
	"smbfsdata",	/* 131 M_SMBFSDATA */ \
	"smbfshash",	/* 132 M_SMBFSHASH */ \
	"sa",		/* 133 M_SA */ \
	"kevent",	/* 134 M_KEVENT */ \
	NULL,		/* 135 */ \
}

struct kmemstats {
	u_long	ks_inuse;	/* # of packets of this type currently in use */
	u_long	ks_calls;	/* total packets of this type ever allocated */
	u_long 	ks_memuse;	/* total memory held in bytes */
	u_short	ks_limblocks;	/* number of times blocked for hitting limit */
	u_short	ks_mapblocks;	/* number of times blocked for kernel map */
	u_long	ks_maxused;	/* maximum number ever used */
	u_long	ks_limit;	/* most that are allowed to exist */
	u_long	ks_size;	/* sizes of this thing that are allocated */
	u_long	ks_spare;
};

/*
 * Array of descriptors that describe the contents of each page
 */
struct kmemusage {
	short ku_indx;		/* bucket index */
	union {
		u_short freecnt;/* for small allocations, free pieces in page */
		u_short pagecnt;/* for large allocations, pages alloced */
	} ku_un;
};
#define	ku_freecnt ku_un.freecnt
#define	ku_pagecnt ku_un.pagecnt

/*
 * Set of buckets for each size of memory block that is retained
 */
struct kmembuckets {
	caddr_t kb_next;	/* list of free blocks */
	caddr_t kb_last;	/* last free block */
	long	kb_calls;	/* total calls to allocate this size */
	long	kb_total;	/* total number of blocks allocated */
	long	kb_totalfree;	/* # of free elements in this bucket */
	long	kb_elmpercl;	/* # of elements in this sized allocation */
	long	kb_highwat;	/* high water mark */
	long	kb_couldfree;	/* over high water mark and could free */
};

#ifdef _KERNEL
#define	MINALLOCSIZE	(1 << MINBUCKET)
#define	BUCKETINDX(size) \
	((size) <= (MINALLOCSIZE * 128) \
		? (size) <= (MINALLOCSIZE * 8) \
			? (size) <= (MINALLOCSIZE * 2) \
				? (size) <= (MINALLOCSIZE * 1) \
					? (MINBUCKET + 0) \
					: (MINBUCKET + 1) \
				: (size) <= (MINALLOCSIZE * 4) \
					? (MINBUCKET + 2) \
					: (MINBUCKET + 3) \
			: (size) <= (MINALLOCSIZE* 32) \
				? (size) <= (MINALLOCSIZE * 16) \
					? (MINBUCKET + 4) \
					: (MINBUCKET + 5) \
				: (size) <= (MINALLOCSIZE * 64) \
					? (MINBUCKET + 6) \
					: (MINBUCKET + 7) \
		: (size) <= (MINALLOCSIZE * 2048) \
			? (size) <= (MINALLOCSIZE * 512) \
				? (size) <= (MINALLOCSIZE * 256) \
					? (MINBUCKET + 8) \
					: (MINBUCKET + 9) \
				: (size) <= (MINALLOCSIZE * 1024) \
					? (MINBUCKET + 10) \
					: (MINBUCKET + 11) \
			: (size) <= (MINALLOCSIZE * 8192) \
				? (size) <= (MINALLOCSIZE * 4096) \
					? (MINBUCKET + 12) \
					: (MINBUCKET + 13) \
				: (size) <= (MINALLOCSIZE * 16384) \
					? (MINBUCKET + 14) \
					: (MINBUCKET + 15))

/*
 * Turn virtual addresses into kmem map indicies
 */
#define	kmemxtob(alloc)	(kmembase + (alloc) * NBPG)
#define	btokmemx(addr)	(((caddr_t)(addr) - kmembase) / NBPG)
#define	btokup(addr)	(&kmemusage[((caddr_t)(addr) - kmembase) >> PGSHIFT])

/*
 * Macro versions for the usual cases of malloc/free
 */
#if defined(KMEMSTATS) || defined(DIAGNOSTIC) || defined(_LKM) || \
    defined(MALLOCLOG) || defined(LOCKDEBUG) || defined(MALLOC_NOINLINE)
#define	MALLOC(space, cast, size, type, flags) \
	(space) = (cast)malloc((u_long)(size), (type), (flags))
#define	FREE(addr, type) free((caddr_t)(addr), (type))

#else /* do not collect statistics */
#define	MALLOC(space, cast, size, type, flags)				\
do {									\
	register struct kmembuckets *__kbp = &bucket[BUCKETINDX((size))]; \
	long __s = splvm();						\
	if (__kbp->kb_next == NULL) {					\
		(space) = (cast)malloc((u_long)(size), (type), (flags)); \
	} else {							\
		(space) = (cast)__kbp->kb_next;				\
		__kbp->kb_next = *(caddr_t *)(space);			\
	}								\
	splx(__s);							\
} while (/* CONSTCOND */ 0)

#define	FREE(addr, type)						\
do {									\
	register struct kmembuckets *__kbp;				\
	register struct kmemusage *__kup = btokup((addr));		\
	long __s = splvm();						\
	if (1 << __kup->ku_indx > MAXALLOCSAVE) {			\
		free((caddr_t)(addr), (type));				\
	} else {							\
		__kbp = &bucket[__kup->ku_indx];			\
		if (__kbp->kb_next == NULL)				\
			__kbp->kb_next = (caddr_t)(addr);		\
		else							\
			*(caddr_t *)(__kbp->kb_last) = (caddr_t)(addr);	\
		*(caddr_t *)(addr) = NULL;				\
		__kbp->kb_last = (caddr_t)(addr);			\
	}								\
	splx(__s);							\
} while(/* CONSTCOND */ 0)
#endif /* do not collect statistics */

extern struct kmemstats		kmemstats[];
extern struct kmemusage		*kmemusage;
extern char			*kmembase;
extern struct kmembuckets	bucket[];

#ifdef MALLOCLOG
void	*_malloc(unsigned long size, int type, int flags,
	    const char *file, long line);
void	_free(void *addr, int type, const char *file, long line);
#define	malloc(size, type, flags) \
	    _malloc((size), (type), (flags), __FILE__, __LINE__)
#define	free(addr, type) \
	    _free((addr), (type), __FILE__, __LINE__)
#else
void	*malloc(unsigned long size, int type, int flags);
void	free(void *addr, int type);
#endif /* MALLOCLOG */

#ifdef MALLOC_DEBUG
int	debug_malloc(unsigned long, int, int, void **);
int	debug_free(void *, int);
void	debug_malloc_init(void);

void	debug_malloc_print(void);
void	debug_malloc_printit(void (*)(const char *, ...), vaddr_t);
#endif /* MALLOC_DEBUG */

void	*realloc(void *curaddr, unsigned long newsize, int type,
	    int flags);
unsigned long
	malloc_roundup(unsigned long);
#endif /* _KERNEL */
#endif /* !_SYS_MALLOC_H_ */

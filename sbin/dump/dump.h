/*	$NetBSD: dump.h,v 1.19.6.5 2002/01/16 09:55:20 he Exp $	*/

/*-
 * Copyright (c) 1980, 1993
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
 *	@(#)dump.h	8.2 (Berkeley) 4/28/95
 */

#include <machine/bswap.h>

#define MAXINOPB	(MAXBSIZE / sizeof(struct dinode))
#define MAXNINDIR	(MAXBSIZE / sizeof(daddr_t))

/*
 * Filestore-independent UFS data, so code can be more easily shared
 * between ffs, lfs, and maybe ext2fs and others as well.
 */
struct ufsi {
	int64_t ufs_dsize;	/* filesystem size, in sectors */
	int32_t ufs_bsize;	/* block size */
	int32_t ufs_bshift;	/* log2(ufs_bsize) */
	int32_t ufs_fsize;	/* fragment size */
	int32_t ufs_frag;	/* block size / frag size */
	int32_t ufs_fsatoda;	/* disk address conversion constant */
	int32_t	ufs_nindir;	/* disk addresses per indirect block */
	int32_t ufs_inopb;	/* inodes per block */
	int32_t ufs_maxsymlinklen; /* max symlink length */
	int32_t ufs_bmask;	/* block mask */
	int32_t ufs_fmask;	/* frag mask */
	int64_t ufs_qbmask;	/* ~ufs_bmask */
	int64_t ufs_qfmask;	/* ~ufs_fmask */
};
#define fsatoda(u,a) ((a) << (u)->ufs_fsatoda)
#define ufs_fragroundup(u,size) /* calculates roundup(size, ufs_fsize) */ \
	(((size) + (u)->ufs_qfmask) & (u)->ufs_fmask)
#define ufs_blkoff(u,loc)   /* calculates (loc % u->ufs_bsize) */ \
	((loc) & (u)->ufs_qbmask)
#define ufs_dblksize(u,d,b) \
	((((b) >= NDADDR || (d)->di_size >= ((b)+1) << (u)->ufs_bshift \
		? (u)->ufs_bsize \
		: (ufs_fragroundup((u), ufs_blkoff(u, (d)->di_size))))))
struct ufsi *ufsib;

/*
 * Dump maps used to describe what is to be dumped.
 */
int	mapsize;	/* size of the state maps */
char	*usedinomap;	/* map of allocated inodes */
char	*dumpdirmap;	/* map of directories to be dumped */
char	*dumpinomap;	/* map of files to be dumped */
/*
 * Map manipulation macros.
 */
#define	SETINO(ino, map) \
	map[(u_int)((ino) - 1) / NBBY] |=  1 << ((u_int)((ino) - 1) % NBBY)
#define	CLRINO(ino, map) \
	map[(u_int)((ino) - 1) / NBBY] &=  ~(1 << ((u_int)((ino) - 1) % NBBY))
#define	TSTINO(ino, map) \
	(map[(u_int)((ino) - 1) / NBBY] &  (1 << ((u_int)((ino) - 1) % NBBY)))

/*
 *	All calculations done in 0.1" units!
 */
char	*disk;		/* name of the disk file */
char	*tape;		/* name of the tape file */
char	*dumpdates;	/* name of the file containing dump date information*/
char	*temp;		/* name of the file for doing rewrite of dumpdates */
char	lastlevel;	/* dump level of previous dump */
char	level;		/* dump level of this dump */
int	uflag;		/* update flag */
int	eflag;		/* eject flag */
int	lflag;		/* autoload flag */
int	diskfd;		/* disk file descriptor */
int	tapefd;		/* tape file descriptor */
int	pipeout;	/* true => output to standard output */
ino_t	curino;		/* current inumber; used globally */
int	newtape;	/* new tape flag */
int	density;	/* density in 0.1" units */
long	tapesize;	/* estimated tape size, blocks */
long	tsize;		/* tape size in 0.1" units */
long	asize;		/* number of 0.1" units written on current tape */
int	etapes;		/* estimated number of tapes */
int	nonodump;	/* if set, do not honor UF_NODUMP user flags */

int	notify;		/* notify operator flag */
int	blockswritten;	/* number of blocks written on current tape */
int	tapeno;		/* current tape number */
time_t	tstart_writing;	/* when started writing the first tape block */
int	xferrate;	/* averaged transfer rate of all volumes */
char	sblock_buf[MAXBSIZE]; /* buffer to hold the superblock */
long	dev_bsize;	/* block size of underlying disk device */
int	dev_bshift;	/* log2(dev_bsize) */
int	tp_bshift;	/* log2(TP_BSIZE) */
int needswap;	/* file system in swapped byte order */
/* some inline functs to help the byte-swapping mess */
static __inline u_int16_t iswap16 __P((u_int16_t));
static __inline u_int32_t iswap32 __P((u_int32_t));
static __inline u_int64_t iswap64 __P((u_int64_t));
    
static __inline u_int16_t iswap16(x)
	u_int16_t x;
{           
	if (needswap)
		return bswap16(x);
	else return x;  
}

static __inline u_int32_t iswap32(x)
    u_int32_t x;
{
	if (needswap)
		return bswap32(x);
	else return x;
}

static __inline u_int64_t iswap64(x)
	u_int64_t x;
{
	if (needswap)
		return bswap64(x);
	else return x;
}  

#ifndef __P
#include <sys/cdefs.h>
#endif

/* filestore-specific hooks */
int	fs_read_sblock __P((char *));
struct ufsi *fs_parametrize __P((void));
ino_t	fs_maxino __P((void));

/* operator interface functions */
void	broadcast __P((char *message));
void	lastdump __P((int arg));	/* int should be char */
void	msg __P((const char *fmt, ...)) __attribute__((__format__(__printf__,1,2)));
void	msgtail __P((const char *fmt, ...)) __attribute__((__format__(__printf__,1,2)));
int	query __P((char *question));
void	quit __P((const char *fmt, ...)) __attribute__((__format__(__printf__,1,2)));
void	set_operators __P((void));
time_t	do_stats __P((void));
void	statussig __P((int));
void	timeest __P((void));
time_t	unctime __P((char *str));

/* mapping routines */
struct	dinode;
long	blockest __P((struct dinode *dp));
void	mapfileino __P((ino_t, long *, int *));
int	mapfiles __P((ino_t maxino, long *tapesize, char *disk,
		    char * const *dirv));
int	mapdirs __P((ino_t maxino, long *tapesize));

/* file dumping routines */
void	blksout __P((daddr_t *blkp, int frags, ino_t ino));
void	dumpino __P((struct dinode *dp, ino_t ino));
void	dumpmap __P((char *map, int type, ino_t ino));
void	writeheader __P((ino_t ino));

/* data block caching */
void	bread __P((daddr_t blkno, char *buf, int size));	
void	rawread __P((daddr_t, char *, int));
void	initcache __P((int, int));
void	printcachestats __P((void));

/* tape writing routines */
int	alloctape __P((void));
void	close_rewind __P((void));
void	dumpblock __P((daddr_t blkno, int size));
void	startnewtape __P((int top));
void	trewind __P((int));
void	writerec __P((char *dp, int isspcl));

void	Exit __P((int status));
void	dumpabort __P((int signo));
void	getfstab __P((void));

char	*rawname __P((char *cp));
struct	dinode *getino __P((ino_t inum));

/* rdump routines */
#ifdef RDUMP
void	rmtclose __P((void));
int	rmthost __P((char *host));
int	rmtopen __P((char *tape, int mode, int verbose));
int	rmtwrite __P((char *buf, int count));
int	rmtioctl(int, int);
#endif /* RDUMP */

void	interrupt __P((int signo));	/* in case operator bangs on console */

/*
 *	Exit status codes
 */
#define	X_FINOK		0	/* normal exit */
#define	X_STARTUP	1	/* startup error */
#define	X_REWRITE	2	/* restart writing from the check point */
#define	X_ABORT		3	/* abort dump; don't attempt checkpointing */

#define	OPGRENT	"operator"		/* group entry to notify */
#define DIALUP	"ttyd"			/* prefix for dialups */

struct	fstab *fstabsearch __P((char *key));	/* search fs_file and fs_spec */

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/*
 *	The contents of the file _PATH_DUMPDATES is maintained both on
 *	a linked list, and then (eventually) arrayified.
 */
struct dumpdates {
	char	dd_name[NAME_MAX+3];
	char	dd_level;
	time_t	dd_ddate;
};
int	nddates;		/* number of records (might be zero) */
struct	dumpdates **ddatev;	/* the arrayfied version */

void	initdumptimes __P((void));
void	getdumptime __P((void));
void	putdumptime __P((void));
#define	ITITERATE(i, ddp) \
	if (ddatev != NULL) \
		for (ddp = ddatev[i = 0]; i < nddates; ddp = ddatev[++i])

void	sig __P((int signo));

/*
 * Compatibility with old systems.
 */
#ifdef COMPAT
#include <sys/file.h>
#define	strchr(a,b)	index(a,b)
#define	strrchr(a,b)	rindex(a,b)
extern char *strdup(), *ctime();
extern int read(), write();
extern int errno;
#endif

#ifndef	_PATH_UTMP
#define	_PATH_UTMP	"/etc/utmp"
#endif
#ifndef	_PATH_FSTAB
#define	_PATH_FSTAB	"/etc/fstab"
#endif

#ifdef sunos
extern char *calloc();
extern char *malloc();
extern long atol();
extern char *strcpy();
extern char *strncpy();
extern char *strcat();
extern time_t time();
extern void endgrent();
extern void exit();
extern off_t lseek();
extern const char *strerror();
#endif

/*	$NetBSD: compat_defs.h,v 1.31.2.4 2004/06/22 21:42:28 tron Exp $	*/

#ifndef	__NETBSD_COMPAT_DEFS_H__
#define	__NETBSD_COMPAT_DEFS_H__

/* Work around some complete brain damage. */

/*
 * Linux: <features.h> turns on _POSIX_SOURCE by default, even though the
 * program (not the OS) should do that.  Preload <features.h> to keep any
 * of this crap from being pulled in, and undefine _POSIX_SOURCE.
 */

#if defined(__linux__) && HAVE_FEATURES_H
#include <features.h>
#endif

/* So _NETBSD_SOURCE doesn't end up defined. Define enough to pull in standard
   defs. Other platforms may need similiar defines. */
#ifdef __NetBSD__
#define _POSIX_SOURCE	1
#define _POSIX_C_SOURCE	200112L
#define _XOPEN_SOURCE 600
#else
#undef _POSIX_SOURCE
#undef _POSIX_C_SOURCE
#endif

/* System headers needed for (re)definitions below. */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
/* time.h needs to be pulled in first at least on netbsd w/o _NETBSD_SOURCE */
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#if HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#if HAVE_SYS_SYSMACROS_H
/* major(), minor() on SVR4 */
#include <sys/sysmacros.h>
#endif
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if HAVE_STDDEF_H
#include <stddef.h>
#endif

/* We don't include <pwd.h> here, so that "compat_pwd.h" works. */
struct passwd;

/* Assume an ANSI compiler for the host. */

#undef __P
#define __P(x) x

#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS
#endif
#ifndef __END_DECLS
#define __END_DECLS
#endif

/* Some things usually in BSD <sys/cdefs.h>. */

#ifndef __CONCAT
#define	__CONCAT(x,y)	x ## y
#endif
#if !defined(__attribute__) && !defined(__GNUC__)
#define __attribute__(x)
#endif
#if !defined(__packed)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#define __packed	__attribute__((__packed__))
#else
#define	__packed	error: no __packed for this compiler
#endif
#endif /* !__packed */
#ifndef __RENAME
#define __RENAME(x)
#endif
#undef __aconst
#define __aconst
#undef __dead
#define __dead
#undef __restrict
#define __restrict

/* Dirent support. */

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) (strlen((dirent)->d_name))
#else
# define dirent direct
# define NAMLEN(dirent) ((dirent)->d_namlen)
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

/* Type substitutes. */

#if !HAVE_ID_T
typedef unsigned long id_t;
#endif

#if !HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#if !HAVE_U_LONG
typedef unsigned long u_long;
#endif

#if !HAVE_U_CHAR
typedef unsigned char u_char;
#endif

#if !HAVE_U_INT
typedef unsigned int u_int;
#endif

#if !HAVE_U_SHORT
typedef unsigned short u_short;
#endif

/* Prototypes for replacement functions. */

#if !HAVE_ATOLL
long long int atoll(const char *);
#endif

#if !HAVE_ASPRINTF
int asprintf(char **, const char *, ...);
#endif

#if !HAVE_ASNPRINTF
int asnprintf(char **, size_t, const char *, ...);
#endif

#if !HAVE_BASENAME
char *basename(char *);
#endif

#if !HAVE_DECL_OPTIND
int getopt(int, char *const *, const char *);
extern char *optarg;
extern int optind, opterr, optopt;
#endif

#if !HAVE_DIRNAME
char *dirname(char *);
#endif

#if !HAVE_DIRFD
#if HAVE_DIR_DD_FD
#define dirfd(dirp) ((dirp)->dd_fd)
#else
/*XXX: Very hacky but no other way to bring this into scope w/o defining
  _NETBSD_SOURCE which we're avoiding. */
#ifdef __NetBSD__
struct _dirdesc {
        int     dd_fd;          /* file descriptor associated with directory */
	long    dd_loc;         /* offset in current buffer */
	long    dd_size;        /* amount of data returned by getdents */
	char    *dd_buf;        /* data buffer */
	int     dd_len;         /* size of data buffer */
	off_t   dd_seek;        /* magic cookie returned by getdents */
	long    dd_rewind;      /* magic cookie for rewinding */
	int     dd_flags;       /* flags for readdir */
	void    *dd_lock;       /* lock for concurrent access */
};
#define dirfd(dirp)     (((struct _dirdesc *)dirp)->dd_fd)
#else
#error cannot figure out how to turn a DIR * into a fd
#endif
#endif
#endif

#if !HAVE_ERR_H
void err(int, const char *, ...);
void errx(int, const char *, ...);
void warn(const char *, ...);
void warnx(const char *, ...);
#endif

#if !HAVE_FGETLN || defined(__NetBSD__)
char *fgetln(FILE *, size_t *);
#endif

#if !HAVE_FLOCK
# define LOCK_SH		0x01
# define LOCK_EX		0x02
# define LOCK_NB		0x04
# define LOCK_UN		0x08
int flock(int, int);
#endif

#if !HAVE_FPARSELN || defined(__NetBSD__)
# define FPARSELN_UNESCESC	0x01
# define FPARSELN_UNESCCONT	0x02
# define FPARSELN_UNESCCOMM	0x04
# define FPARSELN_UNESCREST	0x08
# define FPARSELN_UNESCALL	0x0f
char *fparseln(FILE *, size_t *, size_t *, const char [3], int);
#endif

#if !HAVE_ISSETUGID
int issetugid(void);
#endif

#if !HAVE_ISBLANK && !defined(isblank)
#define isblank(x) ((x) == ' ' || (x) == '\t')
#endif

#if !HAVE_LCHFLAGS
int lchflags(const char *, u_long);
#endif

#if !HAVE_LCHMOD
int lchmod(const char *, mode_t);
#endif

#if !HAVE_LCHOWN
int lchown(const char *, uid_t, gid_t);
#endif

#define __nbcompat_bswap16(x)	((((x) << 8) & 0xff00) | (((x) >> 8) & 0x00ff))

#define __nbcompat_bswap32(x)	((((x) << 24) & 0xff000000) | \
				 (((x) <<  8) & 0x00ff0000) | \
				 (((x) >>  8) & 0x0000ff00) | \
				 (((x) >> 24) & 0x000000ff))

#define __nbcompat_bswap64(x)	(((u_int64_t)bswap32((x)) << 32) | \
				 ((u_int64_t)bswap32((x) >> 32)))

#if !HAVE_BSWAP16
#ifdef bswap16
#undef bswap16
#endif
#define bswap16(x)	__nbcompat_bswap16(x)
#endif
#if !HAVE_BSWAP32
#ifdef bswap32
#undef bswap32
#endif
#define bswap32(x)	__nbcompat_bswap32(x)
#endif
#if !HAVE_BSWAP64
#ifdef bswap64
#undef bswap64
#endif
#define bswap64(x)	__nbcompat_bswap64(x)
#endif

#if !HAVE_MKSTEMP
int mkstemp(char *);
#endif

#if !HAVE_MKDTEMP
char *mkdtemp(char *);
#endif

#if !HAVE_MKSTEMP || !HAVE_MKDTEMP
/* This is a prototype for the internal function. */
int gettemp(char *, int *, int);
#endif

#if !HAVE_PREAD
ssize_t pread(int, void *, size_t, off_t);
#endif

#if !HAVE_PWCACHE_USERDB
int uid_from_user(const char *, uid_t *);
int pwcache_userdb(int (*)(int), void (*)(void),
		struct passwd * (*)(const char *), struct passwd * (*)(uid_t));
int gid_from_group(const char *, gid_t *);
int pwcache_groupdb(int (*)(int), void (*)(void),
		struct group * (*)(const char *), struct group * (*)(gid_t));
#endif
/* Make them use our version */
#  define user_from_uid __nbcompat_user_from_uid
/* Make them use our version */
#  define group_from_gid __nbcompat_group_from_gid

#if !HAVE_PWRITE
ssize_t pwrite(int, const void *, size_t, off_t);
#endif

#if !HAVE_SETENV
int setenv(const char *, const char *, int);
#endif

#if !HAVE_DECL_SETGROUPENT
int setgroupent(int);
#endif

#if !HAVE_DECL_SETPASSENT
int setpassent(int);
#endif

#if !HAVE_SETPROGNAME || defined(__NetBSD__)
const char *getprogname(void);
void setprogname(const char *);
#endif

#if !HAVE_SNPRINTF
int snprintf(char *, size_t, const char *, ...);
#endif

#if !HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif

#if !HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#if !HAVE_STRSEP || defined(__NetBSD__)
char *strsep(char **, const char *);
#endif

#if !HAVE_STRSUFTOLL
long long strsuftoll(const char *, const char *, long long, long long);
long long strsuftollx(const char *, const char *,
			long long, long long, char *, size_t);
#endif

#if !HAVE_STRTOLL
long long strtoll(const char *, char **, int);
#endif

#if !HAVE_USER_FROM_UID
const char *user_from_uid(uid_t, int);
#endif

#if !HAVE_GROUP_FROM_GID
const char *group_from_gid(gid_t, int);
#endif

#if !HAVE_VASPRINTF
int vasprintf(char **, const char *, va_list);
#endif

#if !HAVE_VASNPRINTF
int vasnprintf(char **, size_t, const char *, va_list);
#endif

#if !HAVE_VSNPRINTF
int vsnprintf(char *, size_t, const char *, va_list);
#endif

/*
 * getmode() and setmode() are always defined, as these function names
 * exist but with very different meanings on other OS's.  The compat
 * versions here simply accept an octal mode number; the "u+x,g-w" type
 * of syntax is not accepted.
 */

#define getmode __nbcompat_getmode
#define setmode __nbcompat_setmode

mode_t getmode(const void *, mode_t);
void *setmode(const char *);

/* Eliminate assertions embedded in binaries. */

#undef _DIAGASSERT
#define _DIAGASSERT(x)

/* Various sources use this */
#undef	__RCSID
#define	__RCSID(x)
#undef	__SCCSID
#define	__SCCSID(x)
#undef	__COPYRIGHT
#define	__COPYRIGHT(x)
#undef	__KERNEL_RCSID
#define	__KERNEL_RCSID(x,y)

/* Heimdal expects this one. */

#undef RCSID
#define RCSID(x)

/* Some definitions not available on all systems. */

/* <errno.h> */

#ifndef EFTYPE
#define EFTYPE EIO
#endif

/* <fcntl.h> */

#ifndef O_EXLOCK
#define O_EXLOCK 0
#endif
#ifndef O_SHLOCK
#define O_SHLOCK 0
#endif

/* <limits.h> */

#ifndef UID_MAX
#define UID_MAX 32767
#endif
#ifndef GID_MAX
#define GID_MAX UID_MAX
#endif

#ifndef UQUAD_MAX
#define UQUAD_MAX ((u_quad_t)-1)
#endif
#ifndef QUAD_MAX
#define QUAD_MAX ((quad_t)(UQUAD_MAX >> 1))
#endif
#ifndef QUAD_MIN
#define QUAD_MIN ((quad_t)(~QUAD_MAX))
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX ((unsigned long long)-1)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX ((long long)(ULLONG_MAX >> 1))
#endif
#ifndef LLONG_MIN
#define LLONG_MIN ((long long)(~LLONG_MAX))
#endif

/* <paths.h> */

#ifndef _PATH_BSHELL
#define _PATH_BSHELL PATH_BSHELL
#endif
#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH "/usr/bin:/bin:/usr/local/bin"
#endif
#ifndef _PATH_DEV
#define _PATH_DEV "/dev/"
#endif
#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL _PATH_DEV "null"
#endif
#ifndef _PATH_TMP
#define _PATH_TMP "/tmp/"
#endif
#ifndef _PATH_DEFTAPE
#define _PATH_DEFTAPE "/dev/nrst0"
#endif

/* <stdarg.h> */

#ifndef _BSD_VA_LIST_
#define _BSD_VA_LIST_ va_list
#endif

/* <stdint.h> */

#if !defined(SIZE_MAX) && defined(SIZE_T_MAX)
#define SIZE_MAX SIZE_T_MAX
#endif

#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffffU
#endif

/* <stdlib.h> */

#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#  endif
# endif
#endif

/* avoid prototype conflicts with host */
#define cgetcap __nbcompat_cgetcap
#define cgetclose __nbcompat_cgetclose
#define cgetent __nbcompat_cgetent
#define cgetfirst __nbcompat_cgetfirst
#define cgetmatch __nbcompat_cgetmatch
#define cgetnext __nbcompat_cgetnext
#define cgetnum __nbcompat_cgetnum
#define cgetset __nbcompat_cgetset
#define cgetstr __nbcompat_cgetstr
#define cgetustr __nbcompat_cgetustr

char	*cgetcap(char *, const char *, int);
int	 cgetclose(void);
int	 cgetent(char **, char **, const char *);
int	 cgetfirst(char **, char **);
int	 cgetmatch(const char *, const char *);
int	 cgetnext(char **, char **);
int	 cgetnum(char *, const char *, long *);
int	 cgetset(const char *);
int	 cgetstr(char *, const char *, char **);
int	 cgetustr(char *, const char *, char **);

/* <sys/endian.h> */

#if WORDS_BIGENDIAN
#if !HAVE_HTOBE16
#define htobe16(x)	(x)
#endif
#if !HAVE_HTOBE32
#define htobe32(x)	(x)
#endif
#if !HAVE_HTOBE64
#define htobe64(x)	(x)
#endif
#if !HAVE_HTOLE16
#define htole16(x)	bswap16((u_int16_t)(x))
#endif
#if !HAVE_HTOLE32
#define htole32(x)	bswap32((u_int32_t)(x))
#endif
#if !HAVE_HTOLE64
#define htole64(x)	bswap64((u_int64_t)(x))
#endif
#else
#if !HAVE_HTOBE16
#define htobe16(x)	bswap16((u_int16_t)(x))
#endif
#if !HAVE_HTOBE32
#define htobe32(x)	bswap32((u_int32_t)(x))
#endif
#if !HAVE_HTOBE64
#define htobe64(x)	bswap64((u_int64_t)(x))
#endif
#if !HAVE_HTOLE16
#define htole16(x)	(x)
#endif
#if !HAVE_HTOLE32
#define htole32(x)	(x)
#endif
#if !HAVE_HTOLE64
#define htole64(x)	(x)
#endif
#endif
#if !HAVE_BE16TOH
#define be16toh(x)	htobe16(x)
#endif
#if !HAVE_BE32TOH
#define be32toh(x)	htobe32(x)
#endif
#if !HAVE_BE64TOH
#define be64toh(x)	htobe64(x)
#endif
#if !HAVE_LE16TOH
#define le16toh(x)	htole16(x)
#endif
#if !HAVE_LE32TOH
#define le32toh(x)	htole32(x)
#endif
#if !HAVE_LE64TOH
#define le64toh(x)	htole64(x)
#endif

/* <sys/mman.h> */

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

/* HP-UX has MAP_ANONYMOUS but not MAP_ANON */
#ifndef MAP_ANON
#ifdef MAP_ANONYMOUS
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif

/* <sys/param.h> */

#undef BIG_ENDIAN
#undef LITTLE_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234

#undef BYTE_ORDER
#if WORDS_BIGENDIAN
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#ifndef DEV_BSIZE
#define DEV_BSIZE (1 << 9)
#endif

#undef MIN
#undef MAX
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#ifndef MAXBSIZE
#define MAXBSIZE (64 * 1024)
#endif
#ifndef MAXFRAG
#define MAXFRAG 8
#endif
#ifndef MAXPHYS
#define MAXPHYS (64 * 1024)
#endif

/* XXX needed by makefs; this should be done in a better way */
#undef btodb
#define btodb(x) ((x) << 9)

#undef setbit
#undef clrbit
#undef isset
#undef isclr
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

#ifndef powerof2
#define powerof2(x) ((((x)-1)&(x))==0)
#endif

#undef roundup
#define roundup(x, y)	((((x)+((y)-1))/(y))*(y))

/* <sys/stat.h> */

#ifndef ALLPERMS
#define ALLPERMS (S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
#endif
#ifndef DEFFILEMODE
#define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif
#ifndef S_ISTXT
#ifdef S_ISVTX
#define S_ISTXT S_ISVTX
#else
#define S_ISTXT 0
#endif
#endif

/* Protected by _NETBSD_SOURCE otherwise. */
#if HAVE_STRUCT_STAT_ST_FLAGS && defined(__NetBSD__)
#define UF_SETTABLE     0x0000ffff
#define UF_NODUMP       0x00000001
#define UF_IMMUTABLE    0x00000002
#define UF_APPEND       0x00000004
#define UF_OPAQUE       0x00000008
#define SF_SETTABLE     0xffff0000
#define SF_ARCHIVED     0x00010000
#define SF_IMMUTABLE    0x00020000
#define SF_APPEND       0x00040000
#endif

/* <sys/syslimits.h> */

#ifndef LINE_MAX
#define LINE_MAX 2048
#endif

/* <sys/time.h> */

#ifndef timercmp
#define	timercmp(tvp, uvp, cmp)						\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#endif
#ifndef timeradd
#define	timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (/* CONSTCOND */ 0)
#endif
#ifndef timersub
#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (/* CONSTCOND */ 0)
#endif

/* <sys/types.h> */

#ifdef major
#undef major
#endif
#define major(x)        ((int32_t)((((x) & 0x000fff00) >>  8)))

#ifdef minor
#undef minor
#endif
#define minor(x)        ((int32_t)((((x) & 0xfff00000) >> 12) | \
                                   (((x) & 0x000000ff) >>  0)))
#ifdef makedev
#undef makedev
#endif
#define makedev(x,y)    ((dev_t)((((x) <<  8) & 0x000fff00) | \
			(((y) << 12) & 0xfff00000) | \
			(((y) <<  0) & 0x000000ff)))
#ifndef NBBY
#define NBBY 8
#endif

#if !HAVE_U_QUAD_T
/* #define, not typedef, as quad_t exists as a struct on some systems */
#define quad_t long long
#define u_quad_t unsigned long long
#define strtoq strtoll
#define strtouq strtoull
#endif

/* Has quad_t but these prototypes don't get pulled into scope. w/o we lose */
#ifdef __NetBSD__
quad_t   strtoq __P((const char *, char **, int)); 
u_quad_t strtouq __P((const char *, char **, int)); 
#endif

#endif	/* !__NETBSD_COMPAT_DEFS_H__ */

/*	$NetBSD: config.h,v 1.6.2.1 2000/01/23 12:04:35 he Exp $	*/

/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if type char is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* #undef __CHAR_UNSIGNED__ */
#endif

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define if your struct nlist has an n_un member.  */
/* #undef NLIST_NAME_UNION */

/* Define if you have <nlist.h>.  */
#define NLIST_STRUCT 1

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Package */
#define PACKAGE "xntp3"

/* Version */
#define VERSION "5.92"

/* debugging code */
#define DEBUG 1

/* MD5 authentication */
#define MD5 1

/* DFS authentication (COCOM only) */
#define DES 1

/* reference clock interface */
#define REFCLOCK 1

/* ACTS modem service */
#define ACTS 1

/* Arbiter 1088A/B GPS receiver */
#define ARBITER 1

/* DHD19970505: ARCRON support. */
#define ARCRON_MSF 1

/* Austron 2200A/2201A GPS receiver */
#define AS2201 1

/* PPS interface */
#define ATOM 1

/* Datum/Bancomm bc635/VME interface */
/* #undef BANC */

/* ELV/DCF7000 clock */
#define CLOCK_DCF7000 1

/* HOPF 6021 clock */
#define CLOCK_HOPF6021 1

/* Meinberg clocks */
#define CLOCK_MEINBERG 1

/* DCF77 raw time code */
#define CLOCK_RAWDCF 1

/* RCC 8000 clock */
#define CLOCK_RCC8000 1

/* Schmid DCF77 clock */
#define CLOCK_SCHMID 1

/* Trimble GPS receiver/TAIP protocol */
#define CLOCK_TRIMTAIP 1

/* Trimble GPS receiver/TSIP protocol */
#define CLOCK_TRIMTSIP 1

/* Diems Computime Radio Clock */
#define CLOCK_COMPUTIME 1

/* Datum Programmable Time System */
#define DATUM 1

/* TrueTime GPS receiver/VME interface */
/* #undef GPSVME */

/* Heath GC-1000 WWV/WWVH receiver */
#define HEATH 1

/* HP 58503A GPS receiver */
#define HPGPS 1

/* Sun IRIG audio decoder */
/* #undef IRIG */

/* Leitch CSD 5300 Master Clock System Driver */
#define LEITCH 1

/* local clock reference */
#define LOCAL_CLOCK 1

/* EES M201 MSF receiver */
#define MSFEES 1

/* Magnavox MX4200 GPS receiver */
#define MX4200 1

/* NMEA GPS receiver */
#define NMEA 1

/* PARSE driver interface */
#define PARSE 1

/* PARSE kernel PLL PPS support */
/* #undef PPS_SYNC */

/* PCL 720 clock support */
/* #undef PPS720 */

/* PST/Traconex 1020 WWV/WWVH receiver */
#define PST 1

/* PTB modem service */
#define PTBACTS 1

/* clock thru shared memory */
/* #undef SHM_CLOCK */

/* KSI/Odetics TPRO/S GPS receiver/IRIG interface */
/* #undef TPRO */

/* TRAK 8810 GPS receiver */
#define TRAK 1

/* Kinemetrics/TrueTime receivers */
#define TRUETIME 1

/* USNO modem service */
#define USNO 1

/* Spectracom 8170/Netclock/2 WWVB receiver */
#define WWVB 1

/* define if it's OK to declare char *sys_errlist[]; */
/* #undef CHAR_SYS_ERRLIST */

/* define if it's OK to declare int syscall P((int, struct timeval *, struct timeval *)); */
/* #undef DECL_SYSCALL */

/* define if we have syscall is buggy (Solaris 2.4) */
/* #undef SYSCALL_BUG */

/* Do we need extra room for SO_RCVBUF? (HPUX <8) */
/* #undef NEED_RCVBUF_SLOP */

/* Should we open the broadcast socket? */
#define OPEN_BCAST_SOCKET 1

/* Do we want the HPUX FindConfig()? */
/* #undef NEED_HPUX_FINDCONFIG */

#include <sys/param.h>

/* canonical system (cpu-vendor-os) string */
#define STR_SYSTEM MACHINE_ARCH ## "-unknown-netbsd"

/* define if [gs]ettimeofday() only takes 1 argument */
/* #undef SYSV_TIMEOFDAY */

/* define if struct sockaddr has sa_len */
#define HAVE_SA_LEN_IN_STRUCT_SOCKADDR 1

/* define if struct clockinfo has hz */
#define HAVE_HZ_IN_STRUCT_CLOCKINFO 1

/* define if struct clockinfo has tickadj */
#define  HAVE_TICKADJ_IN_STRUCT_CLOCKINFO 1

/* define if function prototypes are OK */
#define HAVE_PROTOTYPES 1

/* define if setpgrp takes 0 arguments */
/* #undef HAVE_SETPGRP_0 */

/* hardwire a value for tick? */
#define PRESET_TICK 1000000L/hz

/* hardwire a value for tickadj? */
#define PRESET_TICKADJ 500/hz

/* is adjtime() accurate? */
/* #undef ADJTIME_IS_ACCURATE */

/* should we NOT read /dev/kmem? */
#define NOKMEM 1

/* use UDP Wildcard Delivery? */
/* #undef UDP_WILDCARD_DELIVERY */

/* always slew the clock? */
/* #undef SLEWALWAYS */

/* step, then slew the clock? */
/* #undef STEP_SLEW */

/* force ntpdate to step the clock if !defined(STEP_SLEW) ? */
/* #undef FORCE_NTPDATE_STEP */

/* synch TODR hourly? */
/* #undef DOSYNCTODR */

/* do we set process groups with -pid? */
/* #undef UDP_BACKWARDS_SETOWN */

/* must we have a CTTY for fsetown? */
#define USE_FSETOWNCTTY 1

/* can we use SIGIO for tcp and udp IO? */
#define HAVE_SIGNALED_IO 1

/* can we use SIGPOLL for UDP? */
/* #undef USE_UDP_SIGPOLL */

/* can we use SIGPOLL for tty IO? */
/* #undef USE_TTY_SIGPOLL */

/* do we have the chu_clk line discipline/streams module? */
/* #undef CHUCLK */

/* do we have the ppsclock streams module? */
/* #undef PPS */

/* do we have the tty_clk line discipline/streams module? */
/* #undef TTYCLK */

/* does the kernel support precision time discipline? */
#define KERNEL_PLL 1

/* does the kernel support multicasting IP? */
#define MCAST 1

/* do we have ntp_{adj,get}time in libc? */
#define NTP_SYSCALLS_LIBC 1

/* do we have ntp_{adj,get}time in the kernel? */
/* #undef NTP_SYSCALLS_STD */

/* do we have STREAMS/TLI? (Can we replace this with HAVE_SYS_STROPTS_H? */
/* #undef STREAMS_TLI */

/* do we need an s_char typedef? */
#define NEED_S_CHAR_TYPEDEF 1

/* include the GDT Surveying code? */
/* #undef GDT_SURVEYING */

/* does SIOCGIFCONF return size in the buffer? */
/* #undef SIZE_RETURNED_IN_BUFFER */

/* what is the name of TICK in the kernel? */
#define K_TICK_NAME "_tick"

/* Is K_TICK_NAME (nsec_per_tick, for example) in nanoseconds? */
/* #undef TICK_NANO */

/* what is the name of TICKADJ in the kernel? */
#define K_TICKADJ_NAME "_tickadj"

/* Is K_TICKADJ_NAME (hrestime_adj, for example) in nanoseconds? */
/* #undef TICKADJ_NANO */

/* what is (probably) the name of DOSYNCTODR in the kernel? */
#define K_DOSYNCTODR_NAME "_dosynctodr"

/* what is (probably) the name of NOPRINTF in the kernel? */
#define K_NOPRINTF_NAME "_noprintf"

/* do we need HPUX adjtime() library support? */
/* #undef NEED_HPUX_ADJTIME */

/* Might nlist() values require an extra level of indirection (AIX)? */
/* #undef NLIST_EXTRA_INDIRECTION */

/* Should we recommend a minimum value for tickadj? */
/* #undef MIN_REC_TICKADJ */

/* Is there a problem using PARENB and IGNPAR (IRIX)? */
/* #undef NO_PARENB_IGNPAR */

/* Should we not IGNPAR (Linux)? */
/* #undef RAWDCF_NO_IGNPAR */

/* Does DTR power the DCF77 (Linux)? */
/* #undef RAWDCF_SETDTR */

/* Does the compiler like "volatile"? */
/* #undef volatile */

/* Does qsort expect to work on "void *" stuff? */
#define QSORT_USES_VOID_P 1

/* What is the fallback value for HZ? */
#define DEFAULT_HZ 100

/* Do we need to override the system's idea of HZ? */
/* #undef OVERRIDE_HZ */

/* Do we want the SCO3 tickadj hacks? */
/* #undef SCO3_TICKADJ */

/* Do we want the SCO5 tickadj hacks? */
/* #undef SCO5_TICKADJ */

/* adjtime()? */
/* #undef DECL_ADJTIME_0 */

/* bcopy()? */
/* #undef DECL_BCOPY_0 */

/* bzero()? */
/* #undef DECL_BZERO_0 */

/* ioctl()? */
/* #undef DECL_IOCTL_0 */

/* IPC? (bind, connect, recvfrom, sendto, setsockopt, socket) */
/* #undef DECL_IPC_0 */

/* memmove()? */
/* #undef DECL_MEMMOVE_0 */

/* mkstemp()? */
/* #undef DECL_MKSTEMP_0 */

/* mktemp()? */
/* #undef DECL_MKTEMP_0 */

/* plock()? */
/* #undef DECL_PLOCK_0 */

/* rename()? */
/* #undef DECL_RENAME_0 */

/* select()? */
/* #undef DECL_SELECT_0 */

/* setitimer()? */
/* #undef DECL_SETITIMER_0 */

/* setpriority()? */
/* #undef DECL_SETPRIORITY_0 */
/* #undef DECL_SETPRIORITY_1 */

/* sigvec()? */
/* #undef DECL_SIGVEC_0 */

/* stdio stuff? */
/* #undef DECL_STDIO_0 */

/* strtol()? */
/* #undef DECL_STRTOL_0 */

/* syslog() stuff? */
/* #undef DECL_SYSLOG_0 */

/* time()? */
/* #undef DECL_TIME_0 */

/* [gs]ettimeofday()? */
/* #undef DECL_TIMEOFDAY_0 */

/* tolower()? */
/* #undef DECL_TOLOWER_0 */

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a long.  */
/* #undef SIZEOF_LONG 4 */

/* The number of bytes in a signed char.  */
#define SIZEOF_SIGNED_CHAR 1

/* Define if you have the K_open function.  */
/* #undef HAVE_K_OPEN */

/* Define if you have the __adjtimex function.  */
/* #undef HAVE___ADJTIMEX */

/* Define if you have the __ntp_gettime function.  */
/* #undef HAVE___NTP_GETTIME */

/* Define if you have the clock_settime function.  */
#define HAVE_CLOCK_SETTIME 1

/* Define if you have the daemon function.  */
#define HAVE_DAEMON 1

/* Define if you have the getbootfile function.  */
/* #undef HAVE_GETBOOTFILE */

/* Define if you have the getdtablesize function.  */
#define HAVE_GETDTABLESIZE 1

/* Define if you have the getrusage function.  */
#define HAVE_GETRUSAGE 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the getuid function.  */
#define HAVE_GETUID 1

/* Define if you have the kvm_open function.  */
#define HAVE_KVM_OPEN 1

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the memset function.  */
#define HAVE_MEMSET 1

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the mlockall function.  */
/* #undef HAVE_MLOCKALL */

/* Define if you have the nice function.  */
#define HAVE_NICE 1

/* Define if you have the nlist function.  */
#define HAVE_NLIST 1

/* Define if you have the ntp_adjtime function.  */
#define HAVE_NTP_ADJTIME 1

/* Define if you have the ntp_gettime function.  */
#define HAVE_NTP_GETTIME 1

/* Define if you have the plock function.  */
/* #undef HAVE_PLOCK */

/* Define if you have the pututline function.  */
/* #undef HAVE_PUTUTLINE */

/* Define if you have the pututxline function.  */
/* #undef HAVE_PUTUTXLINE */

/* Define if you have the rtprio function.  */
/* #undef HAVE_RTPRIO */

/* Define if you have the sched_setscheduler function.  */
/* #undef HAVE_SCHED_SETSCHEDULER */

/* Define if you have the setlinebuf function.  */
#define HAVE_SETLINEBUF 1

/* Define if you have the setpgid function.  */
#define HAVE_SETPGID 1

/* Define if you have the setpriority function.  */
#define HAVE_SETPRIORITY 1

/* Define if you have the setsid function.  */
#define HAVE_SETSID 1

/* Define if you have the settimeofday function.  */
#define HAVE_SETTIMEOFDAY 1

/* Define if you have the setvbuf function.  */
#define HAVE_SETVBUF 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the sigset function.  */
/* #undef HAVE_SIGSET */

/* Define if you have the sigsuspend function.  */
#define HAVE_SIGSUSPEND 1

/* Define if you have the sigvec function.  */
#define HAVE_SIGVEC 1

/* Define if you have the stime function.  */
/* #undef HAVE_STIME */

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the sysconf function.  */
#define HAVE_SYSCONF 1

/* Define if you have the sysctl function.  */
#define HAVE_SYSCTL 1

/* Define if you have the timer_create function.  */
#define HAVE_TIMER_CREATE 1

/* Define if you have the timer_settime function.  */
/* #undef HAVE_TIMER_SETTIME */

/* Define if you have the umask function.  */
#define HAVE_UMASK 1

/* Define if you have the uname function.  */
#define HAVE_UNAME 1

/* Define if you have the updwtmp function.  */
/* #undef HAVE_UPDWTMP */

/* Define if you have the updwtmpx function.  */
/* #undef HAVE_UPDWTMPX */

/* Define if you have the vsprintf function.  */
#define HAVE_VSPRINTF 1

/* Define if you have the </sys/sync/queue.h> header file.  */
/* #undef HAVE__SYS_SYNC_QUEUE_H */

/* Define if you have the </sys/sync/sema.h> header file.  */
/* #undef HAVE__SYS_SYNC_SEMA_H */

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if <errno.h> does not define the errno variable.  */
/* #undef NEED_DECLARATION_ERRNO */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <machine/inline.h> header file.  */
/* #undef HAVE_MACHINE_INLINE_H */

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <net/if.h> header file.  */
#define HAVE_NET_IF_H 1

/* Define if you have the <netinet/in.h> header file.  */
#define HAVE_NETINET_IN_H 1

/* Define if you have the <netinet/ip.h> header file.  */
#define HAVE_NETINET_IP_H 1

/* Define if you have the <sched.h> header file.  */
/* #undef HAVE_SCHED_H */

/* Define if you have the <sgtty.h> header file.  */
#define HAVE_SGTTY_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/bsd_audioirig.h> header file.  */
/* #undef HAVE_SYS_BSD_AUDIOIRIG_H */

/* Define if you have the <sys/chudefs.h> header file.  */
/* #undef HAVE_SYS_CHUDEFS_H */

/* Define if you have the <sys/clkdefs.h> header file.  */
/* #undef HAVE_SYS_CLKDEFS_H */

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/i8253.h> header file.  */
/* #undef HAVE_SYS_I8253_H */

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/lock.h> header file.  */
#define HAVE_SYS_LOCK_H 1

/* Define if you have the <sys/mman.h> header file.  */
#define HAVE_SYS_MMAN_H 1

/* Define if you have the <sys/modem.h> header file.  */
/* #undef HAVE_SYS_MODEM_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/pcl720.h> header file.  */
/* #undef HAVE_SYS_PCL720_H */

/* Define if you have the <sys/ppsclock.h> header file.  */
/* #undef HAVE_SYS_PPSCLOCK_H */

/* Define if you have the <sys/proc.h> header file.  */
#define HAVE_SYS_PROC_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/sockio.h> header file.  */
#define HAVE_SYS_SOCKIO_H 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/stream.h> header file.  */
/* #undef HAVE_SYS_STREAM_H */

/* Define if you have the <sys/stropts.h> header file.  */
/* #undef HAVE_SYS_STROPTS_H */

/* Define if you have the <sys/sysctl.h> header file.  */
#define HAVE_SYS_SYSCTL_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/timers.h> header file.  */
/* #undef HAVE_SYS_TIMERS_H */

/* Define if you have the <sys/timex.h> header file.  */
#define HAVE_SYS_TIMEX_H 1

/* Define if you have the <sys/tpro.h> header file.  */
/* #undef HAVE_SYS_TPRO_H */

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <termio.h> header file.  */
/* #undef HAVE_TERMIO_H */

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <utmp.h> header file.  */
#define HAVE_UTMP_H 1

/* Define if you have the <utmpx.h> header file.  */
/* #undef HAVE_UTMPX_H */

/* Define if you have the elf library (-lelf).  */
/* #undef HAVE_LIBELF */

/* Define if you have the gen library (-lgen).  */
/* #undef HAVE_LIBGEN */

/* Define if you have the kvm library (-lkvm).  */
#define HAVE_LIBKVM 1

/* Define if you have the ld library (-lld).  */
/* #undef HAVE_LIBLD */

/* Define if you have the mld library (-lmld).  */
/* #undef HAVE_LIBMLD */

/* Define if you have the nsl library (-lnsl).  */
/* #undef HAVE_LIBNSL */

/* Define if you have the posix4 library (-lposix4).  */
/* #undef HAVE_LIBPOSIX4 */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

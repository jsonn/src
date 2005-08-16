dnl aclocal.m4 file for am-utils-6.x
dnl Contains definitions for specialized GNU-autoconf macros.
dnl Author: Erez Zadok <ezk@cs.columbia.edu>
dnl
dnl DO NOT EDIT DIRECTLY!  Generated automatically by maintainers from
dnl m4/GNUmakefile!
dnl
dnl ######################################################################
dnl UNCOMMENT THE NEXT FEW LINES FOR DEBUGGING CONFIGURE
dnl define([AC_CACHE_LOAD], )dnl
dnl define([AC_CACHE_SAVE], )dnl
dnl ======================================================================



dnl ######################################################################
dnl check if compiler can handle "void *"
AC_DEFUN([AMU_C_VOID_P],
[
AC_CACHE_CHECK(if compiler can handle void *,
ac_cv_c_void_p,
[
# try to compile a program which uses void *
AC_TRY_COMPILE(
[ ],
[
void *vp;
], ac_cv_c_void_p=yes, ac_cv_c_void_p=no)
])
if test "$ac_cv_c_void_p" = yes
then
  AC_DEFINE(voidp, void *)
else
  AC_DEFINE(voidp, char *)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl New versions of the cache functions which also dynamically evaluate the
dnl cache-id field, so that it may contain shell variables to expand
dnl dynamically for the creation of $ac_cv_* variables on the fly.
dnl In addition, this function allows you to call COMMANDS which generate
dnl output on the command line, because it prints its own AC_MSG_CHECKING
dnl after COMMANDS are run.
dnl
dnl ======================================================================
dnl AMU_CACHE_CHECK_DYNAMIC(MESSAGE, CACHE-ID, COMMANDS)
define(AMU_CACHE_CHECK_DYNAMIC,
[
ac_tmp=`echo $2`
if eval "test \"`echo '$''{'$ac_tmp'+set}'`\" = set"; then
  AC_MSG_CHECKING([$1])
  echo $ECHO_N "(cached) $ECHO_C" 1>&AS_MESSAGE_FD([])
dnl XXX: for older autoconf versions
dnl  echo $ac_n "(cached) $ac_c" 1>&AS_MESSAGE_FD([])
else
  $3
  AC_MSG_CHECKING([$1])
fi
ac_tmp_val=`eval eval "echo '$''{'$ac_tmp'}'"`
AC_MSG_RESULT($ac_tmp_val)
])
dnl ======================================================================


dnl ######################################################################
dnl check if an automounter filesystem exists (it almost always does).
dnl Usage: AC_CHECK_AMU_FS(<fs>, <msg>, [<depfs>])
dnl Print the message in <msg>, and declare HAVE_AMU_FS_<fs> true.
dnl If <depfs> is defined, then define this filesystem as tru only of the
dnl filesystem for <depfs> is true.
AC_DEFUN([AMU_CHECK_AMU_FS],
[
# store variable name of fs
ac_upcase_am_fs_name=`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=HAVE_AMU_FS_$ac_upcase_am_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for $2 filesystem ($1),
ac_cv_am_fs_$1,
[
# true by default
eval "ac_cv_am_fs_$1=yes"
# if <depfs> exists but is defined to "no", set this filesystem to no.
if test -n "$3"
then
  # flse by default if arg 3 was supplied
  eval "ac_cv_am_fs_$1=no"
  if test "`eval echo '$''{ac_cv_fs_'$3'}'`" = yes
  then
    eval "ac_cv_am_fs_$1=yes"
  fi
  # some filesystems do not have a mnttab entry, but exist based on headers
  if test "`eval echo '$''{ac_cv_fs_header_'$3'}'`" = yes
  then
    eval "ac_cv_am_fs_$1=yes"
  fi
fi
])
# check if need to define variable
if test "`eval echo '$''{ac_cv_am_fs_'$1'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_safe)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check the autofs flavor
AC_DEFUN([AMU_CHECK_AUTOFS_STYLE],
[
AC_CACHE_CHECK(autofs style,
ac_cv_autofs_style,
[
# select the correct style to mount(2) a filesystem
case "${host_os}" in
       solaris1* | solaris2.[[0-4]] )
	       ac_cv_autofs_style=default ;;
       solaris2.5* )
               ac_cv_autofs_style=solaris_v1 ;;
       # Solaris 8+ uses the AutoFS V3/V4 protocols, but they are very similar
       # to V2, so use one style for all.
       solaris* )
               ac_cv_autofs_style=solaris_v2_v3 ;;
       irix6* )
	       ac_cv_autofs_style=solaris_v1 ;;
       linux* )
               ac_cv_autofs_style=linux ;;
       * )
               ac_cv_autofs_style=default ;;
esac
])
# always make a link and include the file name, otherwise on systems where
# autofs support has not been ported yet check_fs_{headers, mntent}.m4 add
# ops_autofs.o to AMD_FS_OBJS, but there's no way to build it.
am_utils_link_files=${am_utils_link_files}amd/ops_autofs.c:conf/autofs/autofs_${ac_cv_autofs_style}.c" "amu_autofs_prot.h:conf/autofs/autofs_${ac_cv_autofs_style}.h" "

# set headers in a macro for Makefile.am files to use (for dependencies)
AMU_AUTOFS_PROT_HEADER='${top_builddir}/'amu_autofs_prot.h
AC_SUBST(AMU_AUTOFS_PROT_HEADER)
])
dnl ======================================================================


dnl ######################################################################
dnl check style of fixmount check_mount() function
AC_DEFUN([AMU_CHECK_CHECKMOUNT_STYLE],
[
AC_CACHE_CHECK(style of fixmount check_mount(),
ac_cv_style_checkmount,
[
# select the correct style for unmounting filesystems
case "${host_os_name}" in
	svr4* | sysv4* | solaris2* | sunos5* )
			ac_cv_style_checkmount=svr4 ;;
	bsd44* | bsdi* | freebsd* | netbsd* | openbsd* | darwin* | rhapsody* )
			ac_cv_style_checkmount=bsd44 ;;
	aix* )
			ac_cv_style_checkmount=aix ;;
	osf* )
			ac_cv_style_checkmount=osf ;;
	ultrix* )
			ac_cv_style_checkmount=ultrix ;;
	* )
			ac_cv_style_checkmount=default ;;
esac
])
am_utils_checkmount_style_file="check_mount.c"
am_utils_link_files=${am_utils_link_files}fixmount/${am_utils_checkmount_style_file}:conf/checkmount/checkmount_${ac_cv_style_checkmount}.c" "

])
dnl ======================================================================


dnl ######################################################################
dnl check for external definition for a function (not external variables)
dnl Usage AMU_CHECK_EXTERN(extern)
dnl Checks for external definition for "extern" that is delimited on the
dnl left and the right by a character that is not a valid symbol character.
dnl
dnl Note that $pattern below is very carefully crafted to match any system
dnl external definition, with __P posix prototypes, with or without an extern
dnl word, etc.  Think twice before changing this.
AC_DEFUN([AMU_CHECK_EXTERN],
[
# store variable name for external definition
ac_upcase_extern_name=`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=HAVE_EXTERN_$ac_upcase_extern_name
# check for cached value and set it if needed
AMU_CACHE_CHECK_DYNAMIC(external function definition for $1,
ac_cv_extern_$1,
[
# the old pattern assumed that the complete external definition is on one
# line but on some systems it is split over several lines, so only match
# beginning of the extern definition including the opening parenthesis.
#pattern="(extern)?.*[^a-zA-Z0-9_]$1[^a-zA-Z0-9_]?.*\(.*\).*;"
pattern="(extern)?.*[[^a-zA-Z0-9_]]$1[[^a-zA-Z0-9_]]?.*\("
AC_EGREP_CPP(${pattern},
[
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else /* not TIME_WITH_SYS_TIME */
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else /* not HAVE_SYS_TIME_H */
#  include <time.h>
# endif /* not HAVE_SYS_TIME_H */
#endif /* not TIME_WITH_SYS_TIME */

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif /* HAVE_STDIO_H */
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if HAVE_STRING_H
# include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif /* HAVE_STRINGS_H */
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif /* HAVE_NETDB_H */
#ifdef HAVE_CLUSTER_H
# include <cluster.h>
#endif /* HAVE_CLUSTER_H */
#ifdef HAVE_RPC_RPC_H
/*
 * Turn on PORTMAP, so that additional header files would get included
 * and the important definition for UDPMSGSIZE is included too.
 */
# ifndef PORTMAP
#  define PORTMAP
# endif /* not PORTMAP */
# include <rpc/rpc.h>
# ifndef XDRPROC_T_TYPE
typedef bool_t (*xdrproc_t) __P ((XDR *, __ptr_t, ...));
# endif /* not XDRPROC_T_TYPE */
#endif /* HAVE_RPC_RPC_H */

#if defined(HAVE_TCPD_H) && defined(HAVE_LIBWRAP)
# include <tcpd.h>
#endif /* defined(HAVE_TCPD_H) && defined(HAVE_LIBWRAP) */

], eval "ac_cv_extern_$1=yes", eval "ac_cv_extern_$1=no")
])
# check if need to define variable
if test "`eval echo '$''{ac_cv_extern_'$1'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_safe)
fi
])
dnl ======================================================================

dnl ######################################################################
dnl run AMU_CHECK_EXTERN on each argument given
dnl Usage: AMU_CHECK_EXTERNS(arg arg arg ...)
AC_DEFUN([AMU_CHECK_EXTERNS],
[
for ac_tmp_arg in $1
do
AMU_CHECK_EXTERN($ac_tmp_arg)
done
])
dnl ======================================================================


dnl ######################################################################
dnl check for external definition for an LDAP function (not external variables)
dnl Usage AMU_CHECK_EXTERN_LDAP(extern)
dnl Checks for external definition for "extern" that is delimited on the
dnl left and the right by a character that is not a valid symbol character.
dnl
dnl Note that $pattern below is very carefully crafted to match any system
dnl external definition, with __P posix prototypes, with or without an extern
dnl word, etc.  Think twice before changing this.
AC_DEFUN([AMU_CHECK_EXTERN_LDAP],
[
# store variable name for external definition
ac_upcase_extern_name=`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=HAVE_EXTERN_$ac_upcase_extern_name
# check for cached value and set it if needed
AMU_CACHE_CHECK_DYNAMIC(external function definition for $1,
ac_cv_extern_$1,
[
# the old pattern assumed that the complete external definition is on one
# line but on some systems it is split over several lines, so only match
# beginning of the extern definition including the opening parenthesis.
#pattern="(extern)?.*[^a-zA-Z0-9_]$1[^a-zA-Z0-9_]?.*\(.*\).*;"
dnl This expression is a bit different than check_extern.m4 because of the
dnl way that openldap wrote their externs in <ldap.h>.
pattern="(extern)?.*([[^a-zA-Z0-9_]])?$1[[^a-zA-Z0-9_]]?.*\("
AC_EGREP_CPP(${pattern},
[
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else /* not TIME_WITH_SYS_TIME */
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else /* not HAVE_SYS_TIME_H */
#  include <time.h>
# endif /* not HAVE_SYS_TIME_H */
#endif /* not TIME_WITH_SYS_TIME */

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif /* HAVE_STDIO_H */
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if HAVE_STRING_H
# include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif /* HAVE_STRINGS_H */
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif /* HAVE_NETDB_H */
#ifdef HAVE_CLUSTER_H
# include <cluster.h>
#endif /* HAVE_CLUSTER_H */
#ifdef HAVE_RPC_RPC_H
/*
 * Turn on PORTMAP, so that additional header files would get included
 * and the important definition for UDPMSGSIZE is included too.
 */
# ifndef PORTMAP
#  define PORTMAP
# endif /* not PORTMAP */
# include <rpc/rpc.h>
# ifndef XDRPROC_T_TYPE
typedef bool_t (*xdrproc_t) __P ((XDR *, __ptr_t, ...));
# endif /* not XDRPROC_T_TYPE */
#endif /* HAVE_RPC_RPC_H */

#if defined(HAVE_TCPD_H) && defined(HAVE_LIBWRAP)
# include <tcpd.h>
#endif /* defined(HAVE_TCPD_H) && defined(HAVE_LIBWRAP) */

#ifdef HAVE_LDAP_H
# include <ldap.h>
#endif /* HAVE_LDAP_H */
#ifdef HAVE_LBER_H
# include <lber.h>
#endif /* HAVE_LBER_H */

], eval "ac_cv_extern_$1=yes", eval "ac_cv_extern_$1=no")
])
# check if need to define variable
if test "`eval echo '$''{ac_cv_extern_'$1'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_safe)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find if type 'fhandle' exists
AC_DEFUN([AMU_CHECK_FHANDLE],
[
AC_CACHE_CHECK(if plain fhandle type exists,
ac_cv_have_fhandle,
[
# try to compile a program which may have a definition for the type
# set to a default value
ac_cv_have_fhandle=no
# look for "struct nfs_fh"
if test "$ac_cv_have_fhandle" = no
then
AC_TRY_COMPILE_NFS(
[ fhandle a;
], ac_cv_have_fhandle=yes, ac_cv_have_fhandle=no)
fi

])
if test "$ac_cv_have_fhandle" != no
then
  AC_DEFINE(HAVE_FHANDLE)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl FIXED VERSION OF AUTOCONF 2.59 AC_CHECK_MEMBER.  g/cc will fail to check
dnl a member if the .member is itself a data structure, because you cannot
dnl compare, in C, a data structure against NULL; you can compare a native
dnl data type (int, char) or a pointer.  Solution: do what I did in my
dnl original member checking macro: try to take the address of the member.
dnl You can always take the address of anything.
dnl -Erez Zadok, Feb 19, 2005.
dnl

# AC_CHECK_MEMBER2(AGGREGATE.MEMBER,
#                 [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
#                 [INCLUDES])
# ---------------------------------------------------------
# AGGREGATE.MEMBER is for instance `struct passwd.pw_gecos', shell
# variables are not a valid argument.
AC_DEFUN([AC_CHECK_MEMBER2],
[AS_LITERAL_IF([$1], [],
	       [AC_FATAL([$0: requires literal arguments])])dnl
m4_bmatch([$1], [\.], ,
	 [m4_fatal([$0: Did not see any dot in `$1'])])dnl
AS_VAR_PUSHDEF([ac_Member], [ac_cv_member_$1])dnl
dnl Extract the aggregate name, and the member name
AC_CACHE_CHECK([for $1], ac_Member,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT([$4])],
[dnl AGGREGATE ac_aggr;
static m4_bpatsubst([$1], [\..*]) ac_aggr;
dnl ac_aggr.MEMBER;
if (&(ac_aggr.m4_bpatsubst([$1], [^[^.]*\.])))
return 0;])],
		[AS_VAR_SET(ac_Member, yes)],
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT([$4])],
[dnl AGGREGATE ac_aggr;
static m4_bpatsubst([$1], [\..*]) ac_aggr;
dnl sizeof ac_aggr.MEMBER;
if (sizeof ac_aggr.m4_bpatsubst([$1], [^[^.]*\.]))
return 0;])],
		[AS_VAR_SET(ac_Member, yes)],
		[AS_VAR_SET(ac_Member, no)])])])
AS_IF([test AS_VAR_GET(ac_Member) = yes], [$2], [$3])dnl
AS_VAR_POPDEF([ac_Member])dnl
])# AC_CHECK_MEMBER


# AC_CHECK_MEMBERS2([AGGREGATE.MEMBER, ...],
#                  [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND]
#                  [INCLUDES])
# ---------------------------------------------------------
# The first argument is an m4 list.
AC_DEFUN([AC_CHECK_MEMBERS2],
[m4_foreach([AC_Member], [$1],
  [AC_CHECK_MEMBER2(AC_Member,
	 [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_[]AC_Member), 1,
			    [Define to 1 if `]m4_bpatsubst(AC_Member,
						     [^[^.]*\.])[' is
			     member of `]m4_bpatsubst(AC_Member, [\..*])['.])
$2],
		 [$3],
		 [$4])])])



dnl ######################################################################
dnl find if structure $1 has field field $2
AC_DEFUN([AMU_CHECK_FIELD],
[
AC_CHECK_MEMBERS2($1, , ,[
AMU_MOUNT_HEADERS(
[
/* now set the typedef */
#ifdef HAVE_STRUCT_MNTENT
typedef struct mntent mntent_t;
#else /* not HAVE_STRUCT_MNTENT */
# ifdef HAVE_STRUCT_MNTTAB
typedef struct mnttab mntent_t;
# endif /*  HAVE_STRUCT_MNTTAB */
#endif /* not HAVE_STRUCT_MNTENT */

/*
 * for various filesystem specific mount arguments
 */

#ifdef HAVE_SYS_FS_PC_FS_H
# include <sys/fs/pc_fs.h>
#endif /* HAVE_SYS_FS_PC_FS_H */
#ifdef HAVE_MSDOSFS_MSDOSFSMOUNT_H
# include <msdosfs/msdosfsmount.h>
#endif /* HAVE_MSDOSFS_MSDOSFSMOUNT_H */
#ifdef HAVE_FS_MSDOSFS_MSDOSFSMOUNT_H
# include <fs/msdosfs/msdosfsmount.h>
#endif /* HAVE_FS_MSDOSFS_MSDOSFSMOUNT_H */

#ifdef HAVE_SYS_FS_EFS_CLNT_H
# include <sys/fs/efs_clnt.h>
#endif /* HAVE_SYS_FS_EFS_CLNT_H */
#ifdef HAVE_SYS_FS_XFS_CLNT_H
# include <sys/fs/xfs_clnt.h>
#endif /* HAVE_SYS_FS_XFS_CLNT_H */
#ifdef HAVE_SYS_FS_UFS_MOUNT_H
# include <sys/fs/ufs_mount.h>
#endif /* HAVE_SYS_FS_UFS_MOUNT_H */
#ifdef HAVE_SYS_FS_AUTOFS_H
# include <sys/fs/autofs.h>
#endif /* HAVE_SYS_FS_AUTOFS_H */
#ifdef HAVE_RPCSVC_AUTOFS_PROT_H
# include <rpcsvc/autofs_prot.h>
#else  /* not HAVE_RPCSVC_AUTOFS_PROT_H */
# ifdef HAVE_SYS_FS_AUTOFS_PROT_H
#  include <sys/fs/autofs_prot.h>
# endif /* HAVE_SYS_FS_AUTOFS_PROT_H */
#endif /* not HAVE_RPCSVC_AUTOFS_PROT_H */
#ifdef HAVE_HSFS_HSFS_H
# include <hsfs/hsfs.h>
#endif /* HAVE_HSFS_HSFS_H */

#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif /* HAVE_IFADDRS_H */

])
])
])
dnl ======================================================================


dnl ######################################################################
dnl check if a filesystem exists (if any of its header files exist).
dnl Usage: AC_CHECK_FS_HEADERS(<headers>..., <fs>, [<fssymbol>])
dnl Check if any of the headers <headers> exist.  If any exist, then
dnl define HAVE_FS_<fs>.  If <fssymbol> exits, then define
dnl HAVE_FS_<fssymbol> instead...
AC_DEFUN([AMU_CHECK_FS_HEADERS],
[
# find what name to give to the fs
if test -n "$3"
then
  ac_fs_name=$3
else
  ac_fs_name=$2
fi
# store variable name of fs
ac_upcase_fs_name=`echo $2 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_fs_headers_safe=HAVE_FS_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for $ac_fs_name filesystem in <$1>,
ac_cv_fs_header_$ac_fs_name,
[
# define to "no" by default
eval "ac_cv_fs_header_$ac_fs_name=no"
# and look to see if it was found
AC_CHECK_HEADERS($1,
[ eval "ac_cv_fs_header_$ac_fs_name=yes"
  break
])])
# check if need to define variable
if test "`eval echo '$''{ac_cv_fs_header_'$ac_fs_name'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_fs_headers_safe)
# append ops_<fs>.o object to AMD_FS_OBJS for automatic compilation
# if first time we add something to this list, then also tell autoconf
# to replace instances of it in Makefiles.
  if test -z "$AMD_FS_OBJS"
  then
    AMD_FS_OBJS="ops_${ac_fs_name}.o"
    AC_SUBST(AMD_FS_OBJS)
  else
    # since this object file could have already been added before
    # we need to ensure we do not add it twice.
    case "${AMD_FS_OBJS}" in
      *ops_${ac_fs_name}.o* ) ;;
      * )
        AMD_FS_OBJS="$AMD_FS_OBJS ops_${ac_fs_name}.o"
      ;;
    esac
  fi
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check if a filesystem type exists (if its header files exist)
dnl Usage: AC_CHECK_FS_MNTENT(<filesystem>, [<fssymbol>])
dnl
dnl Check in some headers for MNTTYPE_<filesystem> macro.  If that exist,
dnl then define HAVE_FS_<filesystem>.  If <fssymbol> exits, then define
dnl HAVE_FS_<fssymbol> instead...
AC_DEFUN([AMU_CHECK_FS_MNTENT],
[
# find what name to give to the fs
if test -n "$2"
then
  ac_fs_name=$2
  ac_fs_as_name=" (from: $1)"
else
  ac_fs_name=$1
  ac_fs_as_name=""
fi
# store variable name of filesystem
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=HAVE_FS_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for $ac_fs_name$ac_fs_as_name mntent definition,
ac_cv_fs_$ac_fs_name,
[
# assume not found
eval "ac_cv_fs_$ac_fs_name=no"
for ac_fs_tmp in $1
do
  ac_upcase_fs_symbol=`echo $ac_fs_tmp | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`

  # first look for MNTTYPE_*
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MNTTYPE_$ac_upcase_fs_symbol
    yes
#endif /* MNTTYPE_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_fs_$ac_fs_name=yes"], [eval "ac_cv_fs_$ac_fs_name=no"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" != no
  then
    break
  fi

  # now try to look for MOUNT_ macro
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MOUNT_$ac_upcase_fs_symbol
    yes
#endif /* MOUNT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_fs_$ac_fs_name=yes"], [eval "ac_cv_fs_$ac_fs_name=no"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" != no
  then
    break
  fi

  # now try to look for MNT_ macro
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MNT_$ac_upcase_fs_symbol
    yes
#endif /* MNT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_fs_$ac_fs_name=yes"], [eval "ac_cv_fs_$ac_fs_name=no"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" != no
  then
    break
  fi

  # now try to look for GT_ macro (ultrix)
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef GT_$ac_upcase_fs_symbol
    yes
#endif /* GT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_fs_$ac_fs_name=yes"], [eval "ac_cv_fs_$ac_fs_name=no"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" != no
  then
    break
  fi

  # look for a loadable filesystem module (linux)
  if test -f /lib/modules/$host_os_version/fs/$ac_fs_tmp.ko
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi
  if test -f /lib/modules/$host_os_version/fs/$ac_fs_tmp.o
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi

  # look for a loadable filesystem module (linux 2.4+)
  if test -f /lib/modules/$host_os_version/kernel/fs/$ac_fs_tmp/$ac_fs_tmp.ko
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi
  if test -f /lib/modules/$host_os_version/kernel/fs/$ac_fs_tmp/$ac_fs_tmp.o
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi

  # look for a loadable filesystem module (linux redhat-5.1)
  if test -f /lib/modules/preferred/fs/$ac_fs_tmp.ko
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi
  if test -f /lib/modules/preferred/fs/$ac_fs_tmp.o
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi

  # in addition look for statically compiled filesystem (linux)
  if egrep "[[^a-zA-Z0-9_]]$ac_fs_tmp$" /proc/filesystems >/dev/null 2>&1
  then
    eval "ac_cv_fs_$ac_fs_name=yes"
    break
  fi

  if test "$ac_fs_tmp" = "nfs3" -a "$ac_cv_header_linux_nfs_mount_h" = "yes"
  then
    # hack hack hack
    # in 6.1, which has fallback to v2/udp, we might want
    # to always use version 4.
    # in 6.0 we do not have much choice
    #
    let nfs_mount_version="`grep NFS_MOUNT_VERSION /usr/include/linux/nfs_mount.h | awk '{print $''3;}'`"
    if test $nfs_mount_version -ge 4
    then
      eval "ac_cv_fs_$ac_fs_name=yes"
      break
    fi
  fi

  # run a test program for bsdi3
  AC_TRY_RUN(
  [
#include <sys/param.h>
#include <sys/mount.h>
main()
{
  int i;
  struct vfsconf vf;
  i = getvfsbyname("$ac_fs_tmp", &vf);
  if (i < 0)
    exit(1);
  else
    exit(0);
}
  ], [eval "ac_cv_fs_$ac_fs_name=yes"
      break
     ]
  )

done
])
# check if need to define variable
if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_safe)
# append ops_<fs>.o object to AMD_FS_OBJS for automatic compilation
# if first time we add something to this list, then also tell autoconf
# to replace instances of it in Makefiles.
  if test -z "$AMD_FS_OBJS"
  then
    AMD_FS_OBJS="ops_${ac_fs_name}.o"
    AC_SUBST(AMD_FS_OBJS)
  else
    # since this object file could have already been added before
    # we need to ensure we do not add it twice.
    case "${AMD_FS_OBJS}" in
      *ops_${ac_fs_name}.o* ) ;;
      * )
        AMD_FS_OBJS="$AMD_FS_OBJS ops_${ac_fs_name}.o"
      ;;
    esac
  fi
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Do we have a GNUish getopt
AC_DEFUN([AMU_CHECK_GNU_GETOPT],
[
AC_CACHE_CHECK([for GNU getopt], ac_cv_sys_gnu_getopt, [
AC_TRY_RUN([
#include <stdio.h>
#include <unistd.h>
int main()
{
   int argc = 3;
   char *argv[] = { "actest", "arg", "-x", NULL };
   int c;
   FILE* rf;
   int isGNU = 0;

   rf = fopen("conftestresult", "w");
   if (rf == NULL) exit(1);

   while ( (c = getopt(argc, argv, "x")) != -1 ) {
       switch ( c ) {
          case 'x':
	     isGNU=1;
             break;
          default:
             exit(1);
       }
   }
   fprintf(rf, isGNU ? "yes" : "no");
   exit(0);
}
],[
ac_cv_sys_gnu_getopt="`cat conftestresult`"
],[
ac_cv_sys_gnu_getopt="fail"
])
])
if test "$ac_cv_sys_gnu_getopt" = "yes"
then
    AC_DEFINE(HAVE_GNU_GETOPT)
fi
])


dnl ######################################################################
dnl Define mount type to hide amd mounts from df(1)
dnl
dnl This has to be determined individually per OS.  Depending on whatever
dnl mount options are defined in the system header files such as
dnl MNTTYPE_IGNORE or MNTTYPE_AUTO, or others does not work: some OSs define
dnl some of these then use other stuff; some do not define them at all in
dnl the headers, but still use it; and more.  After a long attempt to get
dnl this automatically configured, I came to the conclusion that the semi-
dnl automatic per-host-os determination here is the best.
dnl
AC_DEFUN([AMU_CHECK_HIDE_MOUNT_TYPE],
[
AC_CACHE_CHECK(for mount type to hide from df,
ac_cv_hide_mount_type,
[
case "${host_os}" in
	irix* | hpux* )
		ac_cv_hide_mount_type="ignore"
		;;
	sunos4* )
		ac_cv_hide_mount_type="auto"
		;;
	* )
		ac_cv_hide_mount_type="nfs"
		;;
esac
])
AC_DEFINE_UNQUOTED(HIDE_MOUNT_TYPE, "$ac_cv_hide_mount_type")
])
dnl ======================================================================


dnl a bug-fixed version of autoconf 2.12.
dnl first try to link library without $5, and only of that failed,
dnl try with $5 if specified.
dnl it adds $5 to $LIBS if it was needed -Erez.
dnl AC_CHECK_LIB2(LIBRARY, FUNCTION [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND
dnl              [, OTHER-LIBRARIES]]])
AC_DEFUN([AMU_CHECK_LIB2],
[AC_MSG_CHECKING([for $2 in -l$1])
dnl Use a cache variable name containing both the library and function name,
dnl because the test really is for library $1 defining function $2, not
dnl just for library $1.  Separate tests with the same $1 and different $2s
dnl may have different results.
ac_lib_var=`echo $1['_']$2 | sed 'y%./+-%__p_%'`
AC_CACHE_VAL(ac_cv_lib_$ac_lib_var,
[ac_save_LIBS="$LIBS"

# first try with base library, without auxiliary library
LIBS="-l$1 $LIBS"
AC_TRY_LINK(dnl
ifelse([$2], [main], , dnl Avoid conflicting decl of main.
[/* Override any gcc2 internal prototype to avoid an error.  */
]
[/* We use char because int might match the return type of a gcc2
    builtin and then its argument prototype would still apply.  */
char $2();
]),
	    [$2()],
	    eval "ac_cv_lib_$ac_lib_var=\"$1\"",
	    eval "ac_cv_lib_$ac_lib_var=no")

# if OK, set to no auxiliary library, else try auxiliary library
if eval "test \"`echo '$ac_cv_lib_'$ac_lib_var`\" = no"; then
 LIBS="-l$1 $5 $LIBS"
 AC_TRY_LINK(dnl
 ifelse([$2], [main], , dnl Avoid conflicting decl of main.
 [/* Override any gcc2 internal prototype to avoid an error.  */
 ]
 [/* We use char because int might match the return type of a gcc2
     builtin and then its argument prototype would still apply.  */
 char $2();
 ]),
 	    [$2()],
 	    eval "ac_cv_lib_$ac_lib_var=\"$1 $5\"",
 	    eval "ac_cv_lib_$ac_lib_var=no")
fi

LIBS="$ac_save_LIBS"
])dnl
ac_tmp="`eval echo '$''{ac_cv_lib_'$ac_lib_var'}'`"
if test "${ac_tmp}" != no; then
  AC_MSG_RESULT(-l$ac_tmp)
  ifelse([$3], ,
[
  ac_tr_lib=HAVE_LIB`echo $1 | sed -e 's/[[^a-zA-Z0-9_]]/_/g' \
    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'`

  AC_DEFINE_UNQUOTED($ac_tr_lib)
  LIBS="-l$ac_tmp $LIBS"
], [$3])
else
  AC_MSG_RESULT(no)
ifelse([$4], , , [$4
])dnl
fi

])


dnl ######################################################################
dnl check if libwrap (if exists), requires the caller to define the variables
dnl deny_severity and allow_severity.
AC_DEFUN([AMU_CHECK_LIBWRAP_SEVERITY],
[
AC_CACHE_CHECK([if libwrap wants caller to define allow_severity and deny_severity], ac_cv_need_libwrap_severity_vars, [
# save, then reset $LIBS back to original value
SAVEDLIBS="$LIBS"
LIBS="$LIBS -lwrap"
# run program one without defining our own severity variables
AC_TRY_RUN(
[
int main()
{
   exit(0);
}
],[ac_tmp_val1="yes"],[ac_tmp_val1="no"])
# run program two with defining our own severity variables
AC_TRY_RUN(
[
int deny_severity, allow_severity;
int main()
{
   exit(0);
}
],[ac_tmp_val2="yes"],[ac_tmp_val2="no"])
# restore original value of $LIBS
LIBS="$SAVEDLIBS"
# now decide what to do
if test "$ac_tmp_val1" = "no" && test "$ac_tmp_val2" = "yes"
then
	ac_cv_need_libwrap_severity_vars="yes"
else
	ac_cv_need_libwrap_severity_vars="no"
fi
])
if test "$ac_cv_need_libwrap_severity_vars" = "yes"
then
	AC_DEFINE(NEED_LIBWRAP_SEVERITY_VARIABLES)
fi
])


dnl ######################################################################
dnl check if a map exists (if some library function exists).
dnl Usage: AC_CHECK_MAP_FUNCS(<functions>..., <map>, [<mapsymbol>])
dnl Check if any of the functions <functions> exist.  If any exist, then
dnl define HAVE_MAP_<map>.  If <mapsymbol> exits, then defined
dnl HAVE_MAP_<mapsymbol> instead...
AC_DEFUN([AMU_CHECK_MAP_FUNCS],
[
# find what name to give to the map
if test -n "$3"
then
  ac_map_name=$3
else
  ac_map_name=$2
fi
# store variable name of map
ac_upcase_map_name=`echo $ac_map_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=HAVE_MAP_$ac_upcase_map_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for $ac_map_name maps,
ac_cv_map_$ac_map_name,
[
# define to "no" by default
eval "ac_cv_map_$ac_map_name=no"
# and look to see if it was found
AC_CHECK_FUNCS($1,
[
  eval "ac_cv_map_$ac_map_name=yes"
  break
])])
# check if need to define variable
if test "`eval echo '$''{ac_cv_map_'$ac_map_name'}'`" = yes
then
  AC_DEFINE_UNQUOTED($ac_safe)
# append info_<map>.o object to AMD_INFO_OBJS for automatic compilation
# if first time we add something to this list, then also tell autoconf
# to replace instances of it in Makefiles.
  if test -z "$AMD_INFO_OBJS"
  then
    AMD_INFO_OBJS="info_${ac_map_name}.o"
    AC_SUBST(AMD_INFO_OBJS)
  else
    AMD_INFO_OBJS="$AMD_INFO_OBJS info_${ac_map_name}.o"
  fi
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find CDFS-specific mount(2) options (hex numbers)
dnl Usage: AMU_CHECK_MNT2_CDFS_OPT(<fs>)
dnl Check if there is an entry for MS_<fs> or M_<fs> in sys/mntent.h or
dnl mntent.h, then define MNT2_CDFS_OPT_<fs> to the hex number.
AC_DEFUN([AMU_CHECK_MNT2_CDFS_OPT],
[
# what name to give to the fs
ac_fs_name=$1
# store variable name of fs
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=MNT2_CDFS_OPT_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for CDFS-specific mount(2) option $ac_fs_name,
ac_cv_mnt2_cdfs_opt_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mnt2_cdfs_opt_$ac_fs_name=notfound"
value=notfound

# first, try MS_* (most systems).  Must be the first test!
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, MS_$ac_upcase_fs_name)
fi

# if failed, try MNT_* (bsd44 systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, MNT_$ac_upcase_fs_name)
fi

# if failed, try MS_*  as an integer (linux systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_INT(
AMU_MOUNT_HEADERS
, MS_$ac_upcase_fs_name)
fi

# If failed try M_* (must be last test since svr4 systems define M_DATA etc.
# in <sys/stream.h>
# This test was off for now, because of the conflicts with other systems.
# but I turned it back on by faking the inclusion of <sys/stream.h> already.
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
#ifndef _sys_stream_h
# define _sys_stream_h
#endif /* not _sys_stream_h */
#ifndef _SYS_STREAM_H
# define _SYS_STREAM_H
#endif	/* not _SYS_STREAM_H */
AMU_MOUNT_HEADERS
, M_$ac_upcase_fs_name)
fi

# if failed, try ISOFSMNT_* as a hex (bsdi4 systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, ISOFSMNT_$ac_upcase_fs_name)
fi

# set cache variable to value
eval "ac_cv_mnt2_cdfs_opt_$ac_fs_name=$value"
])
# outside cache check, if ok, define macro
ac_tmp=`eval echo '$''{ac_cv_mnt2_cdfs_opt_'$ac_fs_name'}'`
if test "${ac_tmp}" != notfound
then
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================

dnl ######################################################################
dnl run AMU_CHECK_MNT2_CDFS_OPT on each argument given
dnl Usage: AMU_CHECK_MNT2_CDFS_OPTS(arg arg arg ...)
AC_DEFUN([AMU_CHECK_MNT2_CDFS_OPTS],
[
for ac_tmp_arg in $1
do
AMU_CHECK_MNT2_CDFS_OPT($ac_tmp_arg)
done
])
dnl ======================================================================


dnl ######################################################################
dnl Find generic mount(2) options (hex numbers)
dnl Usage: AMU_CHECK_MNT2_GEN_OPT(<fs>)
dnl Check if there is an entry for MS_<fs>, MNT_<fs>, or M_<fs>
dnl (in that order) in mntent.h, sys/mntent.h, or mount.h...
dnl then define MNT2_GEN_OPT_<fs> to the hex number.
AC_DEFUN([AMU_CHECK_MNT2_GEN_OPT],
[
# what name to give to the fs
ac_fs_name=$1
# store variable name of fs
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=MNT2_GEN_OPT_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for generic mount(2) option $ac_fs_name,
ac_cv_mnt2_gen_opt_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mnt2_gen_opt_$ac_fs_name=notfound"
value=notfound

# first, try MS_* (most systems).  Must be the first test!
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, MS_$ac_upcase_fs_name)
fi

# if failed, try MNT_* (bsd44 systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, MNT_$ac_upcase_fs_name)
fi

# if failed, try MS_*  as an integer (linux systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_INT(
AMU_MOUNT_HEADERS
, MS_$ac_upcase_fs_name)
fi

# If failed try M_* (must be last test since svr4 systems define M_DATA etc.
# in <sys/stream.h>
# This test was off for now, because of the conflicts with other systems.
# but I turned it back on by faking the inclusion of <sys/stream.h> already.
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
#ifndef _sys_stream_h
# define _sys_stream_h
#endif /* not _sys_stream_h */
#ifndef _SYS_STREAM_H
# define _SYS_STREAM_H
#endif	/* not _SYS_STREAM_H */
AMU_MOUNT_HEADERS
, M_$ac_upcase_fs_name)
fi

# set cache variable to value
eval "ac_cv_mnt2_gen_opt_$ac_fs_name=$value"
])
# outside cache check, if ok, define macro
ac_tmp=`eval echo '$''{ac_cv_mnt2_gen_opt_'$ac_fs_name'}'`
if test "${ac_tmp}" != notfound
then
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================

dnl ######################################################################
dnl run AMU_CHECK_MNT2_GEN_OPT on each argument given
dnl Usage: AMU_CHECK_MNT2_GEN_OPTS(arg arg arg ...)
AC_DEFUN([AMU_CHECK_MNT2_GEN_OPTS],
[
for ac_tmp_arg in $1
do
AMU_CHECK_MNT2_GEN_OPT($ac_tmp_arg)
done
])
dnl ======================================================================


dnl ######################################################################
dnl Find NFS-specific mount(2) options (hex numbers)
dnl Usage: AMU_CHECK_MNT2_NFS_OPT(<fs>)
dnl Check if there is an entry for NFSMNT_<fs> in sys/mntent.h or
dnl mntent.h, then define MNT2_NFS_OPT_<fs> to the hex number.
AC_DEFUN([AMU_CHECK_MNT2_NFS_OPT],
[
# what name to give to the fs
ac_fs_name=$1
# store variable name of fs
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=MNT2_NFS_OPT_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for NFS-specific mount(2) option $ac_fs_name,
ac_cv_mnt2_nfs_opt_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mnt2_nfs_opt_$ac_fs_name=notfound"
value=notfound

# first try NFSMNT_* (most systems)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, NFSMNT_$ac_upcase_fs_name)
fi

# next try NFS_MOUNT_* (linux)
if test "$value" = notfound
then
AMU_EXPAND_CPP_HEX(
AMU_MOUNT_HEADERS
, NFS_MOUNT_$ac_upcase_fs_name)
fi

# set cache variable to value
eval "ac_cv_mnt2_nfs_opt_$ac_fs_name=$value"
])
# outside cache check, if ok, define macro
ac_tmp=`eval echo '$''{ac_cv_mnt2_nfs_opt_'$ac_fs_name'}'`
if test "${ac_tmp}" != notfound
then
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================

dnl ######################################################################
dnl run AMU_CHECK_MNT2_NFS_OPT on each argument given
dnl Usage: AMU_CHECK_MNT2_NFS_OPTS(arg arg arg ...)
AC_DEFUN([AMU_CHECK_MNT2_NFS_OPTS],
[
for ac_tmp_arg in $1
do
AMU_CHECK_MNT2_NFS_OPT($ac_tmp_arg)
done
])
dnl ======================================================================


dnl ######################################################################
dnl Find name of mount table file, and define it as MNTTAB_FILE_NAME
dnl
dnl Solaris defines MNTTAB as /etc/mnttab, the file where /sbin/mount
dnl stores its cache of mounted filesystems.  But under SunOS, the same
dnl macro MNTTAB, is defined as the _source_ of filesystems to mount, and
dnl is set to /etc/fstab.  That is why I have to first check out
dnl if MOUNTED exists, and if not, check for the MNTTAB macro.
dnl
AC_DEFUN([AMU_CHECK_MNTTAB_FILE_NAME],
[
AC_CACHE_CHECK(for name of mount table file name,
ac_cv_mnttab_file_name,
[
# expand cpp value for MNTTAB
AMU_EXPAND_CPP_STRING(
AMU_MOUNT_HEADERS(
[
/* see M4 comment at the top of the definition of this macro */
#ifdef MOUNTED
# define _MNTTAB_FILE_NAME MOUNTED
# else /* not MOUNTED */
# ifdef MNTTAB
#  define _MNTTAB_FILE_NAME MNTTAB
# endif /* MNTTAB */
#endif /* not MOUNTED */
]),
_MNTTAB_FILE_NAME,
[ ac_cv_mnttab_file_name=$value
],
[
ac_cv_mnttab_file_name=notfound
# check explicitly for /etc/mnttab
if test "$ac_cv_mnttab_file_name" = notfound
then
  if test -f /etc/mnttab
  then
    ac_cv_mnttab_file_name="/etc/mnttab"
  fi
fi
# check explicitly for /etc/mtab
if test "$ac_cv_mnttab_file_name" = notfound
then
  if test -f /etc/mtab
  then
    ac_cv_mnttab_file_name="/etc/mtab"
  fi
fi
])
])
# test value and create macro as needed
if test "$ac_cv_mnttab_file_name" != notfound
then
  AC_DEFINE_UNQUOTED(MNTTAB_FILE_NAME, "$ac_cv_mnttab_file_name")
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check if the mount table is kept in a file or in the kernel.
AC_DEFUN([AMU_CHECK_MNTTAB_LOCATION],
[
AMU_CACHE_CHECK_DYNAMIC(where mount table is kept,
ac_cv_mnttab_location,
[
# assume location is on file
ac_cv_mnttab_location=file
AC_CHECK_FUNCS(mntctl getmntinfo getmountent,
ac_cv_mnttab_location=kernel)
# Solaris 8 Beta Refresh and up use the mntfs pseudo filesystem to store the
# mount table in kernel (cf. mnttab(4): the MS_NOMNTTAB option in
# <sys/mount.h> inhibits that a mount shows up there and thus can be used to
# check for the in-kernel mount table
if test "$ac_cv_mnt2_gen_opt_nomnttab" != notfound
then
  ac_cv_mnttab_location=kernel
fi
])
if test "$ac_cv_mnttab_location" = file
then
 AC_DEFINE(MOUNT_TABLE_ON_FILE)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check the string type of the name of a filesystem mount table entry
dnl option.
dnl Usage: AMU_CHECK_MNTTAB_OPT(<fs>)
dnl Check if there is an entry for MNTOPT_<fs> in sys/mntent.h or mntent.h
dnl define MNTTAB_OPT_<fs> to the string name (e.g., "ro").
AC_DEFUN([AMU_CHECK_MNTTAB_OPT],
[
# what name to give to the fs
ac_fs_name=$1
# store variable name of fs
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=MNTTAB_OPT_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for mount table option $ac_fs_name,
ac_cv_mnttab_opt_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mnttab_opt_$ac_fs_name=notfound"
# and look to see if it was found
AMU_EXPAND_CPP_STRING(
AMU_MOUNT_HEADERS
, MNTOPT_$ac_upcase_fs_name)
# set cache variable to value
if test "${value}" != notfound
then
  eval "ac_cv_mnttab_opt_$ac_fs_name=\\\"$value\\\""
else
  eval "ac_cv_mnttab_opt_$ac_fs_name=$value"
fi
dnl DO NOT CHECK FOR MNT_* b/c bsd44 systems don't use /etc/mnttab,
])
# outside cache check, if ok, define macro
ac_tmp=`eval echo '$''{ac_cv_mnttab_opt_'$ac_fs_name'}'`
if test "${ac_tmp}" != notfound
then
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================

dnl ######################################################################
dnl run AMU_CHECK_MNTTAB_OPT on each argument given
dnl Usage: AMU_CHECK_MNTTAB_OPTS(arg arg arg ...)
AC_DEFUN([AMU_CHECK_MNTTAB_OPTS],
[
for ac_tmp_arg in $1
do
AMU_CHECK_MNTTAB_OPT($ac_tmp_arg)
done
])
dnl ======================================================================


dnl ######################################################################
dnl check style of accessing the mount table file
AC_DEFUN([AMU_CHECK_MNTTAB_STYLE],
[
AC_CACHE_CHECK(mount table style,
ac_cv_style_mnttab,
[
# select the correct style for mount table manipulation functions
case "${host_os_name}" in
	aix* )
			ac_cv_style_mnttab=aix ;;
	bsd* | bsdi* | freebsd* | netbsd* | openbsd* | darwin* | rhapsody* )
			ac_cv_style_mnttab=bsd ;;
	isc3* )
			ac_cv_style_mnttab=isc3 ;;
	mach3* )
			ac_cv_style_mnttab=mach3 ;;
	osf* )
			ac_cv_style_mnttab=osf ;;
	svr4* | sysv4* | solaris2* | sunos5* | aoi* )
			ac_cv_style_mnttab=svr4 ;;
	ultrix* )
			ac_cv_style_mnttab=ultrix ;;
	* )
			ac_cv_style_mnttab=file ;;
esac
])
am_utils_link_files=${am_utils_link_files}libamu/mtabutil.c:conf/mtab/mtab_${ac_cv_style_mnttab}.c" "

# append mtab utilities object to LIBOBJS for automatic compilation
AC_LIBOBJ(mtabutil)
])
dnl ======================================================================


dnl ######################################################################
dnl check the string type of the name of a filesystem mount table entry.
dnl Usage: AC_CHECK_MNTTAB_TYPE(<fs>, [fssymbol])
dnl Check if there is an entry for MNTTYPE_<fs> in sys/mntent.h and mntent.h
dnl define MNTTAB_TYPE_<fs> to the string name (e.g., "nfs").  If <fssymbol>
dnl exist, then define MNTTAB_TYPE_<fssymbol> instead.  If <fssymbol> is
dnl defined, then <fs> can be a list of fs strings to look for.
dnl If no symbols have been defined, but the filesystem has been found
dnl earlier, then set the mount-table type to "<fs>" anyway...
AC_DEFUN([AMU_CHECK_MNTTAB_TYPE],
[
# find what name to give to the fs
if test -n "$2"
then
  ac_fs_name=$2
else
  ac_fs_name=$1
fi
# store variable name of fs
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
ac_safe=MNTTAB_TYPE_$ac_upcase_fs_name
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for mnttab name for $ac_fs_name filesystem,
ac_cv_mnttab_type_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mnttab_type_$ac_fs_name=notfound"
# and look to see if it was found
for ac_fs_tmp in $1
do
  if test "$ac_fs_tmp" = "nfs3" -a "$ac_cv_fs_nfs3" = "yes" -a "$ac_cv_header_linux_nfs_h" = "yes"
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_cv_mnttab_type_nfs\\\""
    break
  fi

  ac_upcase_fs_symbol=`echo $ac_fs_tmp | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' | tr -d '.'`

  # first look for MNTTYPE_*
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MNTTYPE_$ac_upcase_fs_symbol
    yes
#endif /* MNTTYPE_$ac_upcase_fs_symbol */
  ]),
  [ eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
  ])
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_mnttab_type_'$ac_fs_name'}'`" != notfound
  then
    break
  fi

  # look for a loadable filesystem module (linux)
  if test -f /lib/modules/$host_os_version/fs/$ac_fs_tmp.ko
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi
  if test -f /lib/modules/$host_os_version/fs/$ac_fs_tmp.o
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi

  # look for a loadable filesystem module (linux 2.4+)
  if test -f /lib/modules/$host_os_version/kernel/fs/$ac_fs_tmp/$ac_fs_tmp.ko
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi
  if test -f /lib/modules/$host_os_version/kernel/fs/$ac_fs_tmp/$ac_fs_tmp.o
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi

  # look for a loadable filesystem module (linux redhat-5.1)
  if test -f /lib/modules/preferred/fs/$ac_fs_tmp.ko
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi
  if test -f /lib/modules/preferred/fs/$ac_fs_tmp.o
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi

  # next look for statically compiled filesystem (linux)
  if egrep "[[^a-zA-Z0-9_]]$ac_fs_tmp$" /proc/filesystems >/dev/null 2>&1
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi

  # then try to run a program that derefences a static array (bsd44)
  AMU_EXPAND_RUN_STRING(
  AMU_MOUNT_HEADERS(
  [
#ifndef INITMOUNTNAMES
# error INITMOUNTNAMES not defined
#endif /* not INITMOUNTNAMES */
  ]),
  [
  char const *namelist[] = INITMOUNTNAMES;
  if (argc > 1)
    printf("\"%s\"", namelist[MOUNT_$ac_upcase_fs_symbol]);
  ], [ eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$value\\\""
  ])
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_mnttab_type_'$ac_fs_name'}'`" != notfound
  then
    break
  fi

  # finally run a test program for bsdi3
  AC_TRY_RUN(
  [
#include <sys/param.h>
#include <sys/mount.h>
main()
{
  int i;
  struct vfsconf vf;
  i = getvfsbyname("$ac_fs_tmp", &vf);
  if (i < 0)
    exit(1);
  else
    exit(0);
}
  ], [eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
      break
     ]
  )

done

# check if not defined, yet the filesystem is defined
if test "`eval echo '$''{ac_cv_mnttab_type_'$ac_fs_name'}'`" = notfound
then
# this should test if $ac_cv_fs_<fsname> is "yes"
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" = yes ||
    test "`eval echo '$''{ac_cv_fs_header_'$ac_fs_name'}'`" = yes
  then
    eval "ac_cv_mnttab_type_$ac_fs_name=\\\"$ac_fs_name\\\""
  fi
fi
])
# check if need to define variable
ac_tmp=`eval echo '$''{ac_cv_mnttab_type_'$ac_fs_name'}'`
if test "$ac_tmp" != notfound
then
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check style of mounting filesystems
AC_DEFUN([AMU_CHECK_MOUNT_STYLE],
[
AC_CACHE_CHECK(style of mounting filesystems,
ac_cv_style_mount,
[
# select the correct style for mounting filesystems
case "${host_os_name}" in
	solaris1* | sunos[[34]]* | bsdi[[12]]* )
			ac_cv_style_mount=default ;;
	hpux[[6-9]]* | hpux10* )
			ac_cv_style_mount=hpux ;;
	svr4* | sysv4* | solaris* | sunos* | aoi* | hpux* )
			ac_cv_style_mount=svr4 ;;
	bsdi* )
			ac_cv_style_mount=bsdi3 ;;
	aix* )
			ac_cv_style_mount=aix ;;
	irix5* )
			ac_cv_style_mount=irix5 ;;
	irix* )
			ac_cv_style_mount=irix6 ;;
	isc3* )
			ac_cv_style_mount=isc3 ;;
	linux* )
			ac_cv_style_mount=linux ;;
	mach3* )
			ac_cv_style_mount=mach3 ;;
	stellix* )
			ac_cv_style_mount=stellix ;;
	* )	# no style needed.  Use default filesystem calls ala BSD
			ac_cv_style_mount=default ;;
esac
])
am_utils_mount_style_file="mountutil.c"
am_utils_link_files=${am_utils_link_files}libamu/${am_utils_mount_style_file}:conf/mount/mount_${ac_cv_style_mount}.c" "

# append mount utilities object to LIBOBJS for automatic compilation
AC_LIBOBJ(mountutil)
])
dnl ======================================================================


dnl ######################################################################
dnl check the mount system call trap needed to mount(2) a filesystem
AC_DEFUN([AMU_CHECK_MOUNT_TRAP],
[
AC_CACHE_CHECK(mount trap system-call style,
ac_cv_mount_trap,
[
# select the correct style to mount(2) a filesystem
case "${host_os_name}" in
	solaris1* | sunos[[34]]* )
		ac_cv_mount_trap=default ;;
	hpux[[6-9]]* | hpux10* )
		ac_cv_mount_trap=hpux ;;
	svr4* | sysv4* | solaris* | sunos* | aoi* | hpux* )
		ac_cv_mount_trap=svr4 ;;
	news4* | riscix* )
		ac_cv_mount_trap=news4 ;;
	linux* )
		ac_cv_mount_trap=linux ;;
	irix* )
		ac_cv_mount_trap=irix ;;
	aux* )
		ac_cv_mount_trap=aux ;;
	hcx* )
		ac_cv_mount_trap=hcx ;;
	rtu6* )
		ac_cv_mount_trap=rtu6 ;;
	dgux* )
		ac_cv_mount_trap=dgux ;;
	aix* )
		ac_cv_mount_trap=aix3 ;;
	mach2* | mach3* )
		ac_cv_mount_trap=mach3 ;;
	ultrix* )
		ac_cv_mount_trap=ultrix ;;
	isc3* )
		ac_cv_mount_trap=isc3 ;;
	stellix* )
		ac_cv_mount_trap=stellix ;;
	* )
		ac_cv_mount_trap=default ;;
esac
])
am_utils_mount_trap=$srcdir"/conf/trap/trap_"$ac_cv_mount_trap".h"
AC_SUBST_FILE(am_utils_mount_trap)
])
dnl ======================================================================


dnl ######################################################################
dnl check the string type of the name of a filesystem mount table entry.
dnl Usage: AC_CHECK_MOUNT_TYPE(<fs>, [fssymbol])
dnl Check if there is an entry for MNTTYPE_<fs> in sys/mntent.h and mntent.h
dnl define MOUNT_TYPE_<fs> to the string name (e.g., "nfs").  If <fssymbol>
dnl exist, then define MOUNT_TYPE_<fssymbol> instead.  If <fssymbol> is
dnl defined, then <fs> can be a list of fs strings to look for.
dnl If no symbols have been defined, but the filesystem has been found
dnl earlier, then set the mount-table type to "<fs>" anyway...
AC_DEFUN([AMU_CHECK_MOUNT_TYPE],
[
# find what name to give to the fs
if test -n "$2"
then
  ac_fs_name=$2
else
  ac_fs_name=$1
fi
# prepare upper-case name of filesystem
ac_upcase_fs_name=`echo $ac_fs_name | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
##############################################################################
# check for cache and set it if needed
AMU_CACHE_CHECK_DYNAMIC(for mount(2) type/name for $ac_fs_name filesystem,
ac_cv_mount_type_$ac_fs_name,
[
# undefine by default
eval "ac_cv_mount_type_$ac_fs_name=notfound"
# and look to see if it was found
for ac_fs_tmp in $1
do

  ac_upcase_fs_symbol=`echo $ac_fs_tmp | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' | tr -d '.'`

  # first look for MNTTYPE_<fs>
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MNTTYPE_$ac_upcase_fs_symbol
    yes
#endif /* MNTTYPE_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_mount_type_$ac_fs_name=MNTTYPE_$ac_upcase_fs_symbol"],
      [eval "ac_cv_mount_type_$ac_fs_name=notfound"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_mount_type_'$ac_fs_name'}'`" != notfound
  then
    break
  fi

  # next look for MOUNT_<fs>
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MOUNT_$ac_upcase_fs_symbol
    yes
#endif /* MOUNT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_mount_type_$ac_fs_name=MOUNT_$ac_upcase_fs_symbol"],
      [eval "ac_cv_mount_type_$ac_fs_name=notfound"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_mount_type_'$ac_fs_name'}'`" != notfound
  then
    break
  fi

  # next look for MNT_<fs>
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef MNT_$ac_upcase_fs_symbol
    yes
#endif /* MNT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_mount_type_$ac_fs_name=MNT_$ac_upcase_fs_symbol"],
      [eval "ac_cv_mount_type_$ac_fs_name=notfound"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_mount_type_'$ac_fs_name'}'`" != notfound
  then
    break
  fi

  # next look for GT_<fs> (ultrix)
  AC_EGREP_CPP(yes,
  AMU_MOUNT_HEADERS(
  [
#ifdef GT_$ac_upcase_fs_symbol
    yes
#endif /* GT_$ac_upcase_fs_symbol */
  ]), [eval "ac_cv_mount_type_$ac_fs_name=GT_$ac_upcase_fs_symbol"],
      [eval "ac_cv_mount_type_$ac_fs_name=notfound"] )
  # check if need to terminate "for" loop
  if test "`eval echo '$''{ac_cv_mount_type_'$ac_fs_name'}'`" != notfound
  then
    break
  fi

  # look for a loadable filesystem module (linux)
  if test -f /lib/modules/$host_os_version/fs/$ac_fs_tmp.ko
  then
    eval "ac_cv_mount_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi
  if test -f /lib/modules/$host_os_version/fs/$ac_fs_tmp.o
  then
    eval "ac_cv_mount_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi

  # look for a loadable filesystem module (linux 2.4+)
  if test -f /lib/modules/$host_os_version/kernel/fs/$ac_fs_tmp/$ac_fs_tmp.ko
  then
    eval "ac_cv_mount_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi
  if test -f /lib/modules/$host_os_version/kernel/fs/$ac_fs_tmp/$ac_fs_tmp.o
  then
    eval "ac_cv_mount_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi

  # look for a loadable filesystem module (linux redhat-5.1)
  if test -f /lib/modules/preferred/fs/$ac_fs_tmp.ko
  then
    eval "ac_cv_mount_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi
  if test -f /lib/modules/preferred/fs/$ac_fs_tmp.o
  then
    eval "ac_cv_mount_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi

  # in addition look for statically compiled filesystem (linux)
  if egrep "[[^a-zA-Z0-9_]]$ac_fs_tmp$" /proc/filesystems >/dev/null 2>&1
  then
    eval "ac_cv_mount_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
    break
  fi

  # run a test program for bsdi3
  AC_TRY_RUN(
  [
#include <sys/param.h>
#include <sys/mount.h>
main()
{
  int i;
  struct vfsconf vf;
  i = getvfsbyname("$ac_fs_tmp", &vf);
  if (i < 0)
    exit(1);
  else
    exit(0);
}
  ], [eval "ac_cv_mount_type_$ac_fs_name=\\\"$ac_fs_tmp\\\""
      break
     ]
  )

done
# check if not defined, yet the filesystem is defined
if test "`eval echo '$''{ac_cv_mount_type_'$ac_fs_name'}'`" = notfound
then
# this should test if $ac_cv_fs_<fsname> is "yes"
  if test "`eval echo '$''{ac_cv_fs_'$ac_fs_name'}'`" = yes ||
    test "`eval echo '$''{ac_cv_fs_header_'$ac_fs_name'}'`" = yes
  then
    eval "ac_cv_mount_type_$ac_fs_name=MNTTYPE_$ac_upcase_fs_name"
  fi
fi
])
# end of cache check for ac_cv_mount_type_$ac_fs_name
##############################################################################
# check if need to define variable
if test "`eval echo '$''{ac_cv_mount_type_'$ac_fs_name'}'`" != notfound
then
  ac_safe=MOUNT_TYPE_$ac_upcase_fs_name
  ac_tmp=`eval echo '$''{ac_cv_mount_type_'$ac_fs_name'}'`
  AC_DEFINE_UNQUOTED($ac_safe, $ac_tmp)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct printf-style type for the mount type in the mount()
dnl system call.
dnl If you change this one, you must also fix the check_mtype_type.m4.
AC_DEFUN([AMU_CHECK_MTYPE_PRINTF_TYPE],
[
AC_CACHE_CHECK(printf string to print type field of mount() call,
ac_cv_mtype_printf_type,
[
# select the correct printf type
case "${host_os_name}" in
	osf* | freebsd2* | bsdi2* | aix* | ultrix* )
		ac_cv_mtype_printf_type="%d" ;;
	irix3 | isc3 )
		ac_cv_mtype_printf_type="0x%x" ;;
	* )
		ac_cv_mtype_printf_type="%s" ;;
esac
])
AC_DEFINE_UNQUOTED(MTYPE_PRINTF_TYPE, "$ac_cv_mtype_printf_type")
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct type for the mount type in the mount() system call
dnl If you change this one, you must also fix the check_mtype_printf_type.m4.
AC_DEFUN([AMU_CHECK_MTYPE_TYPE],
[
AC_CACHE_CHECK(type of mount type field in mount() call,
ac_cv_mtype_type,
[
# select the correct type
case "${host_os_name}" in
	osf* | freebsd2* | bsdi2* | aix* | ultrix* )
		ac_cv_mtype_type=int ;;
	* )
		ac_cv_mtype_type="char *" ;;
esac
])
AC_DEFINE_UNQUOTED(MTYPE_TYPE, $ac_cv_mtype_type)
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct network transport type to use
AC_DEFUN([AMU_CHECK_NETWORK_TRANSPORT_TYPE],
[
AC_CACHE_CHECK(network transport type,
ac_cv_transport_type,
[
# select the correct type
case "${host_os_name}" in
	solaris1* | sunos[[34]]* | hpux[[6-9]]* | hpux10* )
		ac_cv_transport_type=sockets ;;
	solaris* | sunos* | hpux* )
		ac_cv_transport_type=tli ;;
	* )
		ac_cv_transport_type=sockets ;;
esac
])
am_utils_link_files=${am_utils_link_files}libamu/transputil.c:conf/transp/transp_${ac_cv_transport_type}.c" "

# append transport utilities object to LIBOBJS for automatic compilation
AC_LIBOBJ(transputil)
if test $ac_cv_transport_type = tli
then
  AC_DEFINE(HAVE_TRANSPORT_TYPE_TLI)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct way to dereference the address part of the nfs fhandle
AC_DEFUN([AMU_CHECK_NFS_FH_DREF],
[
AC_CACHE_CHECK(nfs file-handle address dereferencing style,
ac_cv_nfs_fh_dref_style,
[
# select the correct nfs address dereferencing style
case "${host_os}" in
	hpux[[6-9]]* | hpux10* )
		ac_cv_nfs_fh_dref_style=hpux ;;
	sunos3* )
		ac_cv_nfs_fh_dref_style=sunos3 ;;
	sunos4* | solaris1* )
		ac_cv_nfs_fh_dref_style=sunos4 ;;
	svr4* | sysv4* | solaris* | sunos* | hpux* )
		ac_cv_nfs_fh_dref_style=svr4 ;;
	bsd44* | bsdi2* | freebsd2.[[01]]* )
		ac_cv_nfs_fh_dref_style=bsd44 ;;
	# all new BSDs changed the type of the
	# filehandle in nfs_args from nfsv2fh_t to u_char.
	freebsd* | freebsdelf* | bsdi* | netbsd* | openbsd* | darwin* | rhapsody* )
		ac_cv_nfs_fh_dref_style=freebsd22 ;;
	aix[[1-3]]* | aix4.[[01]]* )
		ac_cv_nfs_fh_dref_style=aix3 ;;
	aix* )
		ac_cv_nfs_fh_dref_style=aix42 ;;
	irix* )
		ac_cv_nfs_fh_dref_style=irix ;;
	linux* )
		ac_cv_nfs_fh_dref_style=linux ;;
	isc3 )
		ac_cv_nfs_fh_dref_style=isc3 ;;
	osf[[1-3]]* )
		ac_cv_nfs_fh_dref_style=osf2 ;;
	osf* )
		ac_cv_nfs_fh_dref_style=osf4 ;;
	nextstep* )
		ac_cv_nfs_fh_dref_style=nextstep ;;
	* )
		ac_cv_nfs_fh_dref_style=default ;;
esac
])
am_utils_nfs_fh_dref=$srcdir"/conf/fh_dref/fh_dref_"$ac_cv_nfs_fh_dref_style".h"
AC_SUBST_FILE(am_utils_nfs_fh_dref)
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct way to dereference the hostname part of the nfs fhandle
AC_DEFUN([AMU_CHECK_NFS_HN_DREF],
[
AC_CACHE_CHECK(nfs hostname dereferencing style,
ac_cv_nfs_hn_dref_style,
[
# select the correct nfs address dereferencing style
case "${host_os_name}" in
	linux* )
		ac_cv_nfs_hn_dref_style=linux ;;
	isc3 )
		ac_cv_nfs_hn_dref_style=isc3 ;;
	* )
		ac_cv_nfs_hn_dref_style=default ;;
esac
])
am_utils_nfs_hn_dref=$srcdir"/conf/hn_dref/hn_dref_"$ac_cv_nfs_hn_dref_style".h"
AC_SUBST_FILE(am_utils_nfs_hn_dref)
])
dnl ======================================================================


dnl ######################################################################
dnl check if system has NFS protocol headers
AC_DEFUN([AMU_CHECK_NFS_PROT_HEADERS],
[
AC_CACHE_CHECK(location of NFS protocol header files,
ac_cv_nfs_prot_headers,
[
# select the correct style for mounting filesystems
case "${host_os}" in
	irix5* )
			ac_cv_nfs_prot_headers=irix5 ;;
	irix* )
			ac_cv_nfs_prot_headers=irix6 ;;
	sunos3* )
			ac_cv_nfs_prot_headers=sunos3 ;;
	sunos4* | solaris1* )
			ac_cv_nfs_prot_headers=sunos4 ;;
	sunos5.[[0-3]] | solaris2.[[0-3]] )
			ac_cv_nfs_prot_headers=sunos5_3 ;;
	sunos5.4* | solaris2.4* )
			ac_cv_nfs_prot_headers=sunos5_4 ;;
	sunos5.5* | solaris2.5* )
			ac_cv_nfs_prot_headers=sunos5_5 ;;
	sunos5.6* | solaris2.6* )
			ac_cv_nfs_prot_headers=sunos5_6 ;;
	sunos5.7* | solaris2.7* )
			ac_cv_nfs_prot_headers=sunos5_7 ;;
	sunos5* | solaris2* )
			ac_cv_nfs_prot_headers=sunos5_8 ;;
	bsdi2*)
			ac_cv_nfs_prot_headers=bsdi2 ;;
	bsdi* )
			ac_cv_nfs_prot_headers=bsdi3 ;;
	freebsd2* )
			ac_cv_nfs_prot_headers=freebsd2 ;;
	freebsd* | freebsdelf* )
			ac_cv_nfs_prot_headers=freebsd3 ;;
	netbsd1.[[0-2]]* )
			ac_cv_nfs_prot_headers=netbsd ;;
	netbsd1.3* )
			ac_cv_nfs_prot_headers=netbsd1_3 ;;
	netbsd* | netbsdelf* )
			ac_cv_nfs_prot_headers=netbsd1_4 ;;
	openbsd* )
			ac_cv_nfs_prot_headers=openbsd ;;
	hpux[[6-9]]* | hpux10* )
			ac_cv_nfs_prot_headers=hpux ;;
	hpux* )
			ac_cv_nfs_prot_headers=hpux11 ;;
	aix[[1-3]]* )
			ac_cv_nfs_prot_headers=aix3 ;;
	aix4.[[01]]* )
			ac_cv_nfs_prot_headers=aix4 ;;
	aix4.2* )
			ac_cv_nfs_prot_headers=aix4_2 ;;
	aix4.3* )
			ac_cv_nfs_prot_headers=aix4_3 ;;
	aix5.1* )
			ac_cv_nfs_prot_headers=aix5_1 ;;
	aix* )
			ac_cv_nfs_prot_headers=aix5_2 ;;
	osf[[1-3]]* )
			ac_cv_nfs_prot_headers=osf2 ;;
	osf4* )
			ac_cv_nfs_prot_headers=osf4 ;;
	osf* )
			ac_cv_nfs_prot_headers=osf5 ;;
	svr4* )
			ac_cv_nfs_prot_headers=svr4 ;;
	sysv4* )	# this is for NCR2 machines
			ac_cv_nfs_prot_headers=ncr2 ;;
	linux* )
			ac_cv_nfs_prot_headers=linux ;;
	nextstep* )
			ac_cv_nfs_prot_headers=nextstep ;;
	ultrix* )
			ac_cv_nfs_prot_headers=ultrix ;;
 	darwin* | rhapsody* )
 			ac_cv_nfs_prot_headers=darwin ;;
	* )
			ac_cv_nfs_prot_headers=default ;;
esac
])

# make sure correct header is linked in top build directory
am_utils_nfs_prot_file="amu_nfs_prot.h"
am_utils_link_files=${am_utils_link_files}${am_utils_nfs_prot_file}:conf/nfs_prot/nfs_prot_${ac_cv_nfs_prot_headers}.h" "

# define the name of the header to be included for other M4 macros
AC_DEFINE_UNQUOTED(AMU_NFS_PROTOCOL_HEADER, "${srcdir}/conf/nfs_prot/nfs_prot_${ac_cv_nfs_prot_headers}.h")

# set headers in a macro for Makefile.am files to use (for dependencies)
AMU_NFS_PROT_HEADER='${top_builddir}/'$am_utils_nfs_prot_file
AC_SUBST(AMU_NFS_PROT_HEADER)
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct way to dereference the address part of the nfs fhandle
AC_DEFUN([AMU_CHECK_NFS_SA_DREF],
[
AC_CACHE_CHECK(nfs address dereferencing style,
ac_cv_nfs_sa_dref_style,
[
# select the correct nfs address dereferencing style
case "${host_os}" in
	hpux[[6-9]]* | hpux10* | sunos[[34]]* | solaris1* )
		ac_cv_nfs_sa_dref_style=default ;;
	svr4* | sysv4* | solaris* | sunos* | hpux* )
		ac_cv_nfs_sa_dref_style=svr4 ;;
	386bsd* | bsdi1* )
		ac_cv_nfs_sa_dref_style=386bsd ;;
	bsd44* | bsdi* | freebsd* | netbsd* | openbsd* | darwin* | rhapsody* )
		ac_cv_nfs_sa_dref_style=bsd44 ;;
	linux* )
		ac_cv_nfs_sa_dref_style=linux ;;
	aix* )
		ac_cv_nfs_sa_dref_style=aix3 ;;
	aoi* )
		ac_cv_nfs_sa_dref_style=aoi ;;
	isc3 )
		ac_cv_nfs_sa_dref_style=isc3 ;;
	* )
		ac_cv_nfs_sa_dref_style=default ;;
esac
])
am_utils_nfs_sa_dref=$srcdir"/conf/sa_dref/sa_dref_"$ac_cv_nfs_sa_dref_style".h"
AC_SUBST_FILE(am_utils_nfs_sa_dref)
])
dnl ======================================================================


dnl ######################################################################
dnl check if need to turn on, off, or leave alone the NFS "noconn" option
AC_DEFUN([AMU_CHECK_NFS_SOCKET_CONNECTION],
[
AC_CACHE_CHECK(if to turn on/off noconn option,
ac_cv_nfs_socket_connection,
[
# set default to no-change
ac_cv_nfs_socket_connection=none
# select the correct style
case "${host_os}" in
	openbsd2.[[01]]* )
			ac_cv_nfs_socket_connection=noconn ;;
	openbsd* | freebsd* | freebsdelf* )
			ac_cv_nfs_socket_connection=conn ;;
esac
])
# set correct value
case "$ac_cv_nfs_socket_connection" in
	noconn )
		AC_DEFINE(USE_UNCONNECTED_NFS_SOCKETS)
		;;
	conn )
		AC_DEFINE(USE_CONNECTED_NFS_SOCKETS)
		;;
esac
])
dnl ======================================================================


dnl ######################################################################
dnl set OS libraries specific to an OS:
dnl libnsl/libsocket are needed only on solaris and some svr4 systems.
dnl Using a typical macro has proven unsuccesful, because on some other
dnl systems such as irix, including libnsl and or libsocket actually breaks
dnl lots of code.  So I am forced to use a special purpose macro that sets
dnl the libraries based on the OS.  Sigh.  -Erez.
AC_DEFUN([AMU_CHECK_OS_LIBS],
[
AC_CACHE_CHECK(for additional OS libraries,
ac_cv_os_libs,
[
# select the correct set of libraries to link with
case "${host_os_name}" in
	svr4* | sysv4* | solaris2* | sunos5* | aoi* )
			ac_cv_os_libs="-lsocket -lnsl" ;;
	* )
			ac_cv_os_libs=none ;;
esac
])
# set list of libraries to link with
if test "$ac_cv_os_libs" != none
then
  LIBS="$ac_cv_os_libs $LIBS"
fi

])
dnl ======================================================================


dnl ######################################################################
dnl check if a system needs to restart its signal handlers
AC_DEFUN([AMU_CHECK_RESTARTABLE_SIGNAL_HANDLER],
[
AC_CACHE_CHECK(if system needs to restart signal handlers,
ac_cv_restartable_signal_handler,
[
# select the correct systems to restart signal handlers
case "${host_os_name}" in
	svr3* | svr4* | sysv4* | solaris2* | sunos5* | aoi* | irix* )
			ac_cv_restartable_signal_handler=yes ;;
	* )
			ac_cv_restartable_signal_handler=no ;;
esac
])
# define REINSTALL_SIGNAL_HANDLER if need to
if test "$ac_cv_restartable_signal_handler" = yes
then
  AC_DEFINE(REINSTALL_SIGNAL_HANDLER)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check style of unmounting filesystems
AC_DEFUN([AMU_CHECK_UMOUNT_STYLE],
[
AC_CACHE_CHECK(style of unmounting filesystems,
ac_cv_style_umount,
[
# select the correct style for unmounting filesystems
case "${host_os_name}" in
	linux* )
			ac_cv_style_umount=linux ;;
	bsd44* | bsdi* | freebsd* | netbsd* | openbsd* | darwin* | rhapsody* )
			ac_cv_style_umount=bsd44 ;;
	osf* )
			ac_cv_style_umount=osf ;;
	* )
			ac_cv_style_umount=default ;;
esac
])
am_utils_umount_style_file="umount_fs.c"
am_utils_link_files=${am_utils_link_files}libamu/${am_utils_umount_style_file}:conf/umount/umount_${ac_cv_style_umount}.c" "

# append un-mount utilities object to LIBOBJS for automatic compilation
AC_LIBOBJ(umount_fs)
])
dnl ======================================================================


dnl ######################################################################
dnl check the unmount system call arguments needed for
AC_DEFUN([AMU_CHECK_UNMOUNT_ARGS],
[
AC_CACHE_CHECK(unmount system-call arguments,
ac_cv_unmount_args,
[
# select the correct style to mount(2) a filesystem
case "${host_os_name}" in
	aix* )
		ac_cv_unmount_args="mnt->mnt_passno, 0" ;;
	ultrix* )
		ac_cv_unmount_args="mnt->mnt_passno" ;;
	* )
		ac_cv_unmount_args="mnt->mnt_dir" ;;
esac
])
am_utils_unmount_args=$ac_cv_unmount_args
AC_SUBST(am_utils_unmount_args)
])
dnl ======================================================================


dnl ######################################################################
dnl check for the correct system call to unmount a filesystem.
AC_DEFUN([AMU_CHECK_UNMOUNT_CALL],
[
dnl make sure this one is called before [AC_CHECK_UNMOUNT_ARGS]
AC_BEFORE([$0], [AC_CHECK_UNMOUNT_ARGS])
AC_CACHE_CHECK(the system call to unmount a filesystem,
ac_cv_unmount_call,
[
# check for various unmount a filesystem calls
if test "$ac_cv_func_uvmount" = yes ; then
  ac_cv_unmount_call=uvmount
elif test "$ac_cv_func_unmount" = yes ; then
  ac_cv_unmount_call=unmount
elif test "$ac_cv_func_umount" = yes ; then
  ac_cv_unmount_call=umount
else
  ac_cv_unmount_call=no
fi
])
if test "$ac_cv_unmount_call" != no
then
  am_utils_unmount_call=$ac_cv_unmount_call
  AC_SUBST(am_utils_unmount_call)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Expand the value of a CPP macro into a printable hex number.
dnl Takes: header, macro, [action-if-found, [action-if-not-found]]
dnl It runs the header through CPP looking for a match between the macro
dnl and a string pattern, and if sucessful, it prints the string value out.
AC_DEFUN([AMU_EXPAND_CPP_HEX],
[
# we are looking for a regexp of a string
AC_EGREP_CPP(0x,
[$1]
$2,
value="notfound"
AC_TRY_RUN(
[
[$1]
main(argc)
int argc;
{
#ifdef $2
if (argc > 1)
  printf("0x%x", $2);
exit(0);
#else
# error no such option $2
#endif
exit(1);
}], value=`./conftest dummy 2>>config.log`, value="notfound", value="notfound")
,
value="notfound"
)
if test "$value" = notfound
then
  :
  $4
else
  :
  $3
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Expand the value of a CPP macro into a printable integer number.
dnl Takes: header, macro, [action-if-found, [action-if-not-found]]
dnl It runs the header through CPP looking for a match between the macro
dnl and a string pattern, and if sucessful, it prints the string value out.
AC_DEFUN([AMU_EXPAND_CPP_INT],
[
# we are looking for a regexp of an integer (must not start with 0 --- those
# are octals).
AC_EGREP_CPP(
[[1-9]][[0-9]]*,
[$1]
$2,
value="notfound"
AC_TRY_RUN(
[
[$1]
main(argc)
int argc;
{
#ifdef $2
if (argc > 1)
  printf("%d", $2);
exit(0);
#else
# error no such option $2
#endif
exit(1);
}], value=`./conftest dummy 2>>config.log`, value="notfound", value="notfound")
,
value="notfound"
)
if test "$value" = notfound
then
  :
  $4
else
  :
  $3
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Expand the value of a CPP macro into a printable string.
dnl Takes: header, macro, [action-if-found, [action-if-not-found]]
dnl It runs the header through CPP looking for a match between the macro
dnl and a string pattern, and if sucessful, it prints the string value out.
AC_DEFUN([AMU_EXPAND_CPP_STRING],
[
# we are looking for a regexp of a string
AC_EGREP_CPP(\".*\",
[$1]
$2,
value="notfound"
AC_TRY_RUN(
[
[$1]
main(argc)
int argc;
{
#ifdef $2
if (argc > 1)
  printf("%s", $2);
exit(0);
#else
# error no such option $2
#endif
exit(1);
}], value=`./conftest dummy 2>>config.log`, value="notfound", value="notfound")
,
value="notfound"
)
if test "$value" = notfound
then
  :
  $4
else
  :
  $3
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Run a program and print its output as a string
dnl Takes: (header, code-to-run, [action-if-found, [action-if-not-found]])
AC_DEFUN([AMU_EXPAND_RUN_STRING],
[
value="notfound"
AC_TRY_RUN(
[
$1
main(argc)
int argc;
{
$2
exit(0);
}], value=`./conftest dummy 2>>config.log`, value="notfound", value="notfound")
if test "$value" = notfound
then
  :
  $4
else
  :
  $3
fi
])
dnl ======================================================================


dnl ######################################################################
dnl find if "extern char *optarg" exists in headers
AC_DEFUN([AMU_EXTERN_OPTARG],
[
AC_CACHE_CHECK(if external definition for optarg[] exists,
ac_cv_extern_optarg,
[
# try to compile program that uses the variable
AC_TRY_COMPILE(
[
#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif /* HAVE_STDIO_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif /* HAVE_SYS_ERRNO_H */
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif /* HAVE_ERRNO_H */
],
[
char *cp = optarg;
], ac_cv_extern_optarg=yes, ac_cv_extern_optarg=no)
])
if test "$ac_cv_extern_optarg" = yes
then
  AC_DEFINE(HAVE_EXTERN_OPTARG)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl find if "extern char *sys_errlist[]" exist in headers
AC_DEFUN([AMU_EXTERN_SYS_ERRLIST],
[
AC_CACHE_CHECK(if external definition for sys_errlist[] exists,
ac_cv_extern_sys_errlist,
[
# try to locate pattern in header files
#pattern="(extern)?.*char.*sys_errlist.*\[\]"
pattern="(extern)?.*char.*sys_errlist.*"
AC_EGREP_CPP(${pattern},
[
#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif /* HAVE_STDIO_H */
#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif /* HAVE_SYS_ERRNO_H */
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif /* HAVE_ERRNO_H */
], ac_cv_extern_sys_errlist=yes, ac_cv_extern_sys_errlist=no)
])
# check if need to define variable
if test "$ac_cv_extern_sys_errlist" = yes
then
  AC_DEFINE(HAVE_EXTERN_SYS_ERRLIST)
fi
])
dnl ======================================================================


fdnl ######################################################################
dnl find if mntent_t field mnt_time exists and is of type "char *"
AC_DEFUN([AMU_FIELD_MNTENT_T_MNT_TIME_STRING],
[
AC_CACHE_CHECK(if mntent_t field mnt_time exist as type string,
ac_cv_field_mntent_t_mnt_time_string,
[
# try to compile a program
AC_TRY_COMPILE(
AMU_MOUNT_HEADERS(
[
/* now set the typedef */
#ifdef HAVE_STRUCT_MNTENT
typedef struct mntent mntent_t;
#else /* not HAVE_STRUCT_MNTENT */
# ifdef HAVE_STRUCT_MNTTAB
typedef struct mnttab mntent_t;
# else /* not HAVE_STRUCT_MNTTAB */
#  error XXX: could not find definition for struct mntent or struct mnttab!
# endif /* not HAVE_STRUCT_MNTTAB */
#endif /* not HAVE_STRUCT_MNTENT */
]),
[
mntent_t mtt;
char *cp = "test";
int i;
mtt.mnt_time = cp;
i = mtt.mnt_time[0];
], ac_cv_field_mntent_t_mnt_time_string=yes, ac_cv_field_mntent_t_mnt_time_string=no)
])
if test "$ac_cv_field_mntent_t_mnt_time_string" = yes
then
  AC_DEFINE(HAVE_MNTENT_T_MNT_TIME_STRING)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Check if we have as buggy hasmntopt() libc function
AC_DEFUN([AMU_FUNC_BAD_HASMNTOPT],
[
AC_CACHE_CHECK([for working hasmntopt], ac_cv_func_hasmntopt_working,
[AC_TRY_RUN(
AMU_MOUNT_HEADERS(
[[
#ifdef HAVE_MNTENT_H
/* some systems need <stdio.h> before <mntent.h> is included */
# ifdef HAVE_STDIO_H
#  include <stdio.h>
# endif /* HAVE_STDIO_H */
# include <mntent.h>
#endif /* HAVE_MNTENT_H */
#ifdef HAVE_SYS_MNTENT_H
# include <sys/mntent.h>
#endif /* HAVE_SYS_MNTENT_H */
#ifdef HAVE_SYS_MNTTAB_H
# include <sys/mnttab.h>
#endif /* HAVE_SYS_MNTTAB_H */
#if defined(HAVE_MNTTAB_H) && !defined(MNTTAB)
# include <mnttab.h>
#endif /* defined(HAVE_MNTTAB_H) && !defined(MNTTAB) */
#ifdef HAVE_STRUCT_MNTENT
typedef struct mntent mntent_t;
#else /* not HAVE_STRUCT_MNTENT */
# ifdef HAVE_STRUCT_MNTTAB
typedef struct mnttab mntent_t;
/* map struct mnttab field names to struct mntent field names */
#  define mnt_opts	mnt_mntopts
# endif /* not HAVE_STRUCT_MNTTAB */
#endif /* not HAVE_STRUCT_MNTENT */

int main()
{
  mntent_t mnt;
  char *tmp = NULL;

 /*
  * Test if hasmntopt will incorrectly find the string "soft", which
  * is part of the large "softlookup" function.
  */
  mnt.mnt_opts = "hard,softlookup,ro";

  if ((tmp = hasmntopt(&mnt, "soft")))
    exit(1);
  exit(0);
}
]]),
	[ac_cv_func_hasmntopt_working=yes],
	[ac_cv_func_hasmntopt_working=no]
)])
if test $ac_cv_func_hasmntopt_working = no
then
	AC_LIBOBJ([hasmntopt])
 	AC_DEFINE(HAVE_BAD_HASMNTOPT)
fi
])


dnl My version is similar to the one from Autoconf 2.52, but I also
dnl define HAVE_BAD_MEMCMP so that I can do smarter things to avoid
dnl linkage conflicts with bad memcmp versions that are in libc.
AC_DEFUN([AMU_FUNC_BAD_MEMCMP],
[
AC_FUNC_MEMCMP
if test "$ac_cv_func_memcmp_working" = no
then
AC_DEFINE(HAVE_BAD_MEMCMP)
fi
])


dnl Check for a yp_all() function that does not leak a file descriptor
dnl to the ypserv process.
AC_DEFUN([AMU_FUNC_BAD_YP_ALL],
[
AC_CACHE_CHECK(for a file-descriptor leakage clean yp_all,
ac_cv_func_yp_all_clean,
[
# clean by default
ac_cv_func_yp_all_clean=yes
# select the correct type
case "${host_os_name}" in
	irix* )
		ac_cv_func_yp_all_clean=no ;;
	linux* )
		# RedHat 5.1 systems with glibc glibc-2.0.7-19 or below
		# leak a UDP socket from yp_all()
		case "`cat /etc/redhat-release /dev/null 2>/dev/null`" in
			*5.1* )
				ac_cv_func_yp_all_clean=no ;;
		esac
esac
])
if test $ac_cv_func_yp_all_clean = no
then
  AC_DEFINE(HAVE_BAD_YP_ALL)
fi
])


dnl FILE: m4/macros/header_templates.m4
dnl defines descriptions for various am-utils specific macros

AH_TEMPLATE([HAVE_AMU_FS_AUTO],
[Define if have automount filesystem])

AH_TEMPLATE([HAVE_AMU_FS_DIRECT],
[Define if have direct automount filesystem])

AH_TEMPLATE([HAVE_AMU_FS_TOPLVL],
[Define if have "top-level" filesystem])

AH_TEMPLATE([HAVE_AMU_FS_ERROR],
[Define if have error filesystem])

AH_TEMPLATE([HAVE_AMU_FS_PROGRAM],
[Define if have program filesystem])

AH_TEMPLATE([HAVE_AMU_FS_LINK],
[Define if have symbolic-link filesystem])

AH_TEMPLATE([HAVE_AMU_FS_LINKX],
[Define if have symlink with existence check filesystem])

AH_TEMPLATE([HAVE_AMU_FS_HOST],
[Define if have NFS host-tree filesystem])

AH_TEMPLATE([HAVE_AMU_FS_NFSL],
[Define if have nfsl (NFS with local link check) filesystem])

AH_TEMPLATE([HAVE_AMU_FS_NFSX],
[Define if have multi-NFS filesystem])

AH_TEMPLATE([HAVE_AMU_FS_UNION],
[Define if have union filesystem])

AH_TEMPLATE([HAVE_MAP_FILE],
[Define if have file maps (everyone should have it!)])

AH_TEMPLATE([HAVE_MAP_NIS],
[Define if have NIS maps])

AH_TEMPLATE([HAVE_MAP_NISPLUS],
[Define if have NIS+ maps])

AH_TEMPLATE([HAVE_MAP_DBM],
[Define if have DBM maps])

AH_TEMPLATE([HAVE_MAP_NDBM],
[Define if have NDBM maps])

AH_TEMPLATE([HAVE_MAP_HESIOD],
[Define if have HESIOD maps])

AH_TEMPLATE([HAVE_MAP_LDAP],
[Define if have LDAP maps])

AH_TEMPLATE([HAVE_MAP_PASSWD],
[Define if have PASSWD maps])

AH_TEMPLATE([HAVE_MAP_UNION],
[Define if have UNION maps])

AH_TEMPLATE([HAVE_MAP_EXEC],
[Define if have executable maps])

AH_TEMPLATE([HAVE_FS_UFS],
[Define if have UFS filesystem])

AH_TEMPLATE([HAVE_FS_XFS],
[Define if have XFS filesystem (irix)])

AH_TEMPLATE([HAVE_FS_EFS],
[Define if have EFS filesystem (irix)])

AH_TEMPLATE([HAVE_FS_NFS],
[Define if have NFS filesystem])

AH_TEMPLATE([HAVE_FS_NFS3],
[Define if have NFS3 filesystem])

AH_TEMPLATE([HAVE_FS_PCFS],
[Define if have PCFS filesystem])

AH_TEMPLATE([HAVE_FS_LOFS],
[Define if have LOFS filesystem])

AH_TEMPLATE([HAVE_FS_HSFS],
[Define if have HSFS filesystem])

AH_TEMPLATE([HAVE_FS_CDFS],
[Define if have CDFS filesystem])

AH_TEMPLATE([HAVE_FS_TFS],
[Define if have TFS filesystem])

AH_TEMPLATE([HAVE_FS_TMPFS],
[Define if have TMPFS filesystem])

AH_TEMPLATE([HAVE_FS_MFS],
[Define if have MFS filesystem])

AH_TEMPLATE([HAVE_FS_CFS],
[Define if have CFS (crypto) filesystem])

AH_TEMPLATE([HAVE_FS_AUTOFS],
[Define if have AUTOFS filesystem])

AH_TEMPLATE([HAVE_FS_CACHEFS],
[Define if have CACHEFS filesystem])

AH_TEMPLATE([HAVE_FS_NULLFS],
[Define if have NULLFS (loopback on bsd44) filesystem])

AH_TEMPLATE([HAVE_FS_UNIONFS],
[Define if have UNIONFS filesystem])

AH_TEMPLATE([HAVE_FS_UMAPFS],
[Define if have UMAPFS (uid/gid mapping) filesystem])

AH_TEMPLATE([MOUNT_TYPE_UFS],
[Mount(2) type/name for UFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_XFS],
[Mount(2) type/name for XFS filesystem (irix)])

AH_TEMPLATE([MOUNT_TYPE_EFS],
[Mount(2) type/name for EFS filesystem (irix)])

AH_TEMPLATE([MOUNT_TYPE_NFS],
[Mount(2) type/name for NFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_NFS3],
[Mount(2) type/name for NFS3 filesystem])

AH_TEMPLATE([MOUNT_TYPE_PCFS],
[Mount(2) type/name for PCFS filesystem. XXX: conf/trap/trap_hpux.h may override this definition for HPUX 9.0])

AH_TEMPLATE([MOUNT_TYPE_LOFS],
[Mount(2) type/name for LOFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_CDFS],
[Mount(2) type/name for CDFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_TFS],
[Mount(2) type/name for TFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_TMPFS],
[Mount(2) type/name for TMPFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_MFS],
[Mount(2) type/name for MFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_CFS],
[Mount(2) type/name for CFS (crypto) filesystem])

AH_TEMPLATE([MOUNT_TYPE_AUTOFS],
[Mount(2) type/name for AUTOFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_CACHEFS],
[Mount(2) type/name for CACHEFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_IGNORE],
[Mount(2) type/name for IGNORE filesystem (not real just ignore for df)])

AH_TEMPLATE([MOUNT_TYPE_NULLFS],
[Mount(2) type/name for NULLFS (loopback on bsd44) filesystem])

AH_TEMPLATE([MOUNT_TYPE_UNIONFS],
[Mount(2) type/name for UNIONFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_UMAPFS],
[Mount(2) type/name for UMAPFS (uid/gid mapping) filesystem])

AH_TEMPLATE([MNTTAB_TYPE_UFS],
[Mount-table entry name for UFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_XFS],
[Mount-table entry name for XFS filesystem (irix)])

AH_TEMPLATE([MNTTAB_TYPE_EFS],
[Mount-table entry name for EFS filesystem (irix)])

AH_TEMPLATE([MNTTAB_TYPE_NFS],
[Mount-table entry name for NFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_NFS3],
[Mount-table entry name for NFS3 filesystem])

AH_TEMPLATE([MNTTAB_TYPE_PCFS],
[Mount-table entry name for PCFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_LOFS],
[Mount-table entry name for LOFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_CDFS],
[Mount-table entry name for CDFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_TFS],
[Mount-table entry name for TFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_TMPFS],
[Mount-table entry name for TMPFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_MFS],
[Mount-table entry name for MFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_CFS],
[Mount-table entry name for CFS (crypto) filesystem])

AH_TEMPLATE([MNTTAB_TYPE_AUTOFS],
[Mount-table entry name for AUTOFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_CACHEFS],
[Mount-table entry name for CACHEFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_NULLFS],
[Mount-table entry name for NULLFS (loopback on bsd44) filesystem])

AH_TEMPLATE([MNTTAB_TYPE_UNIONFS],
[Mount-table entry name for UNIONFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_UMAPFS],
[Mount-table entry name for UMAPFS (uid/gid mapping) filesystem])

AH_TEMPLATE([MNTTAB_FILE_NAME],
[Name of mount table file name])

AH_TEMPLATE([HIDE_MOUNT_TYPE],
[Name of mount type to hide amd mount from df(1)])

AH_TEMPLATE([MNTTAB_OPT_RO],
[Mount Table option string: Read only])

AH_TEMPLATE([MNTTAB_OPT_RW],
[Mount Table option string: Read/write])

AH_TEMPLATE([MNTTAB_OPT_RQ],
[Mount Table option string: Read/write with quotas])

AH_TEMPLATE([MNTTAB_OPT_QUOTA],
[Mount Table option string: Check quotas])

AH_TEMPLATE([MNTTAB_OPT_NOQUOTA],
[Mount Table option string: Don't check quotas])

AH_TEMPLATE([MNTTAB_OPT_ONERROR],
[Mount Table option string: action to taken on error])

AH_TEMPLATE([MNTTAB_OPT_TOOSOON],
[Mount Table option string: min. time between inconsistencies])

AH_TEMPLATE([MNTTAB_OPT_SOFT],
[Mount Table option string: Soft mount])

AH_TEMPLATE([MNTTAB_OPT_SPONGY],
[Mount Table option string: spongy mount])

AH_TEMPLATE([MNTTAB_OPT_HARD],
[Mount Table option string: Hard mount])

AH_TEMPLATE([MNTTAB_OPT_SUID],
[Mount Table option string: Set uid allowed])

AH_TEMPLATE([MNTTAB_OPT_NOSUID],
[Mount Table option string: Set uid not allowed])

AH_TEMPLATE([MNTTAB_OPT_GRPID],
[Mount Table option string: SysV-compatible gid on create])

AH_TEMPLATE([MNTTAB_OPT_REMOUNT],
[Mount Table option string: Change mount options])

AH_TEMPLATE([MNTTAB_OPT_NOSUB],
[Mount Table option string: Disallow mounts on subdirs])

AH_TEMPLATE([MNTTAB_OPT_MULTI],
[Mount Table option string: Do multi-component lookup])

AH_TEMPLATE([MNTTAB_OPT_INTR],
[Mount Table option string: Allow NFS ops to be interrupted])

AH_TEMPLATE([MNTTAB_OPT_NOINTR],
[Mount Table option string: Don't allow interrupted ops])

AH_TEMPLATE([MNTTAB_OPT_PORT],
[Mount Table option string: NFS server IP port number])

AH_TEMPLATE([MNTTAB_OPT_SECURE],
[Mount Table option string: Secure (AUTH_DES) mounting])

AH_TEMPLATE([MNTTAB_OPT_KERB],
[Mount Table option string: Secure (AUTH_Kerb) mounting])

AH_TEMPLATE([MNTTAB_OPT_RSIZE],
[Mount Table option string: Max NFS read size (bytes)])

AH_TEMPLATE([MNTTAB_OPT_WSIZE],
[Mount Table option string: Max NFS write size (bytes)])

AH_TEMPLATE([MNTTAB_OPT_TIMEO],
[Mount Table option string: NFS timeout (1/10 sec)])

AH_TEMPLATE([MNTTAB_OPT_RETRANS],
[Mount Table option string: Max retransmissions (soft mnts)])

AH_TEMPLATE([MNTTAB_OPT_ACTIMEO],
[Mount Table option string: Attr cache timeout (sec)])

AH_TEMPLATE([MNTTAB_OPT_ACREGMIN],
[Mount Table option string: Min attr cache timeout (files)])

AH_TEMPLATE([MNTTAB_OPT_ACREGMAX],
[Mount Table option string: Max attr cache timeout (files)])

AH_TEMPLATE([MNTTAB_OPT_ACDIRMIN],
[Mount Table option string: Min attr cache timeout (dirs)])

AH_TEMPLATE([MNTTAB_OPT_ACDIRMAX],
[Mount Table option string: Max attr cache timeout (dirs)])

AH_TEMPLATE([MNTTAB_OPT_NOAC],
[Mount Table option string: Don't cache attributes at all])

AH_TEMPLATE([MNTTAB_OPT_NOCTO],
[Mount Table option string: No close-to-open consistency])

AH_TEMPLATE([MNTTAB_OPT_BG],
[Mount Table option string: Do mount retries in background])

AH_TEMPLATE([MNTTAB_OPT_FG],
[Mount Table option string: Do mount retries in foreground])

AH_TEMPLATE([MNTTAB_OPT_RETRY],
[Mount Table option string: Number of mount retries])

AH_TEMPLATE([MNTTAB_OPT_DEV],
[Mount Table option string: Device id of mounted fs])

AH_TEMPLATE([MNTTAB_OPT_FSID],
[Mount Table option string: Filesystem id of mounted fs])

AH_TEMPLATE([MNTTAB_OPT_POSIX],
[Mount Table option string: Get static pathconf for mount])

AH_TEMPLATE([MNTTAB_OPT_PRIVATE],
[Mount Table option string: Use local locking])

AH_TEMPLATE([MNTTAB_OPT_MAP],
[Mount Table option string: Automount map])

AH_TEMPLATE([MNTTAB_OPT_DIRECT],
[Mount Table option string: Automount   direct map mount])

AH_TEMPLATE([MNTTAB_OPT_INDIRECT],
[Mount Table option string: Automount indirect map mount])

AH_TEMPLATE([MNTTAB_OPT_LLOCK],
[Mount Table option string: Local locking (no lock manager)])

AH_TEMPLATE([MNTTAB_OPT_IGNORE],
[Mount Table option string: Ignore this entry])

AH_TEMPLATE([MNTTAB_OPT_NOAUTO],
[Mount Table option string: No auto (what?)])

AH_TEMPLATE([MNTTAB_OPT_NOCONN],
[Mount Table option string: No connection])

AH_TEMPLATE([MNTTAB_OPT_VERS],
[Mount Table option string: protocol version number indicator])

AH_TEMPLATE([MNTTAB_OPT_PROTO],
[Mount Table option string: protocol network_id indicator])

AH_TEMPLATE([MNTTAB_OPT_SYNCDIR],
[Mount Table option string: Synchronous local directory ops])

AH_TEMPLATE([MNTTAB_OPT_NOSETSEC],
[Mount Table option string: Do no allow setting sec attrs])

AH_TEMPLATE([MNTTAB_OPT_SYMTTL],
[Mount Table option string: set symlink cache time-to-live])

AH_TEMPLATE([MNTTAB_OPT_COMPRESS],
[Mount Table option string: compress])

AH_TEMPLATE([MNTTAB_OPT_PGTHRESH],
[Mount Table option string: paging threshold])

AH_TEMPLATE([MNTTAB_OPT_MAXGROUPS],
[Mount Table option string: max groups])

AH_TEMPLATE([MNTTAB_OPT_PROPLIST],
[Mount Table option string: support property lists (ACLs)])

AH_TEMPLATE([MNT2_GEN_OPT_ASYNC],
[asynchronous filesystem access])

AH_TEMPLATE([MNT2_GEN_OPT_AUTOMNTFS],
[automounter filesystem (ignore) flag, used in bsdi-4.1])

AH_TEMPLATE([MNT2_GEN_OPT_AUTOMOUNTED],
[automounter filesystem flag, used in Mac OS X / Darwin])

AH_TEMPLATE([MNT2_GEN_OPT_BIND],
[directory hardlink])

AH_TEMPLATE([MNT2_GEN_OPT_CACHE],
[cache (what?)])

AH_TEMPLATE([MNT2_GEN_OPT_DATA],
[6-argument mount])

AH_TEMPLATE([MNT2_GEN_OPT_FSS],
[old (4-argument) mount (compatibility)])

AH_TEMPLATE([MNT2_GEN_OPT_IGNORE],
[ignore mount entry in df output])

AH_TEMPLATE([MNT2_GEN_OPT_JFS],
[journaling filesystem (AIX's UFS/FFS)])

AH_TEMPLATE([MNT2_GEN_OPT_GRPID],
[old BSD group-id on create])

AH_TEMPLATE([MNT2_GEN_OPT_MULTI],
[do multi-component lookup on files])

AH_TEMPLATE([MNT2_GEN_OPT_NEWTYPE],
[use type string instead of int])

AH_TEMPLATE([MNT2_GEN_OPT_NFS],
[NFS mount])

AH_TEMPLATE([MNT2_GEN_OPT_NOCACHE],
[nocache (what?)])

AH_TEMPLATE([MNT2_GEN_OPT_NODEV],
[do not interpret special device files])

AH_TEMPLATE([MNT2_GEN_OPT_NOEXEC],
[no exec calls allowed])

AH_TEMPLATE([MNT2_GEN_OPT_NONDEV],
[do not interpret special device files])

AH_TEMPLATE([MNT2_GEN_OPT_NOSUB],
[Disallow mounts beneath this mount])

AH_TEMPLATE([MNT2_GEN_OPT_NOSUID],
[Setuid programs disallowed])

AH_TEMPLATE([MNT2_GEN_OPT_NOTRUNC],
[Return ENAMETOOLONG for long filenames])

AH_TEMPLATE([MNT2_GEN_OPT_OPTIONSTR],
[Pass mount option string to kernel])

AH_TEMPLATE([MNT2_GEN_OPT_OVERLAY],
[allow overlay mounts])

AH_TEMPLATE([MNT2_GEN_OPT_QUOTA],
[check quotas])

AH_TEMPLATE([MNT2_GEN_OPT_RDONLY],
[Read-only])

AH_TEMPLATE([MNT2_GEN_OPT_REMOUNT],
[change options on an existing mount])

AH_TEMPLATE([MNT2_GEN_OPT_RONLY],
[read only])

AH_TEMPLATE([MNT2_GEN_OPT_SYNC],
[synchronize data immediately to filesystem])

AH_TEMPLATE([MNT2_GEN_OPT_SYNCHRONOUS],
[synchronous filesystem access (same as SYNC)])

AH_TEMPLATE([MNT2_GEN_OPT_SYS5],
[Mount with Sys 5-specific semantics])

AH_TEMPLATE([MNT2_GEN_OPT_UNION],
[Union mount])

AH_TEMPLATE([MNT2_NFS_OPT_AUTO],
[hide mount type from df(1)])

AH_TEMPLATE([MNT2_NFS_OPT_ACDIRMAX],
[set max secs for dir attr cache])

AH_TEMPLATE([MNT2_NFS_OPT_ACDIRMIN],
[set min secs for dir attr cache])

AH_TEMPLATE([MNT2_NFS_OPT_ACREGMAX],
[set max secs for file attr cache])

AH_TEMPLATE([MNT2_NFS_OPT_ACREGMIN],
[set min secs for file attr cache])

AH_TEMPLATE([MNT2_NFS_OPT_AUTHERR],
[Authentication error])

AH_TEMPLATE([MNT2_NFS_OPT_DEADTHRESH],
[set dead server retry thresh])

AH_TEMPLATE([MNT2_NFS_OPT_DISMINPROG],
[Dismount in progress])

AH_TEMPLATE([MNT2_NFS_OPT_DISMNT],
[Dismounted])

AH_TEMPLATE([MNT2_NFS_OPT_DUMBTIMR],
[Don't estimate rtt dynamically])

AH_TEMPLATE([MNT2_NFS_OPT_GRPID],
[System V-style gid inheritance])

AH_TEMPLATE([MNT2_NFS_OPT_HASAUTH],
[Has authenticator])

AH_TEMPLATE([MNT2_NFS_OPT_FSNAME],
[provide name of server's fs to system])

AH_TEMPLATE([MNT2_NFS_OPT_HOSTNAME],
[set hostname for error printf])

AH_TEMPLATE([MNT2_NFS_OPT_IGNORE],
[ignore mount point])

AH_TEMPLATE([MNT2_NFS_OPT_INT],
[allow interrupts on hard mount])

AH_TEMPLATE([MNT2_NFS_OPT_INTR],
[allow interrupts on hard mount])

AH_TEMPLATE([MNT2_NFS_OPT_INTERNAL],
[Bits set internally])

AH_TEMPLATE([MNT2_NFS_OPT_KERB],
[Use Kerberos authentication])

AH_TEMPLATE([MNT2_NFS_OPT_KERBEROS],
[use kerberos credentials])

AH_TEMPLATE([MNT2_NFS_OPT_KNCONF],
[transport's knetconfig structure])

AH_TEMPLATE([MNT2_NFS_OPT_LEASETERM],
[set lease term (nqnfs)])

AH_TEMPLATE([MNT2_NFS_OPT_LLOCK],
[Local locking (no lock manager)])

AH_TEMPLATE([MNT2_NFS_OPT_MAXGRPS],
[set maximum grouplist size])

AH_TEMPLATE([MNT2_NFS_OPT_MNTD],
[Mnt server for mnt point])

AH_TEMPLATE([MNT2_NFS_OPT_MYWRITE],
[Assume writes were mine])

AH_TEMPLATE([MNT2_NFS_OPT_NFSV3],
[mount NFS Version 3])

AH_TEMPLATE([MNT2_NFS_OPT_NOAC],
[don't cache attributes])

AH_TEMPLATE([MNT2_NFS_OPT_NOCONN],
[Don't Connect the socket])

AH_TEMPLATE([MNT2_NFS_OPT_NOCTO],
[no close-to-open consistency])

AH_TEMPLATE([MNT2_NFS_OPT_NOINT],
[disallow interrupts on hard mounts])

AH_TEMPLATE([MNT2_NFS_OPT_NQLOOKLEASE],
[Get lease for lookup])

AH_TEMPLATE([MNT2_NFS_OPT_NONLM],
[Don't use locking])

AH_TEMPLATE([MNT2_NFS_OPT_NQNFS],
[Use Nqnfs protocol])

AH_TEMPLATE([MNT2_NFS_OPT_POSIX],
[static pathconf kludge info])

AH_TEMPLATE([MNT2_NFS_OPT_PRIVATE],
[Use local locking])

AH_TEMPLATE([MNT2_NFS_OPT_RCVLOCK],
[Rcv socket lock])

AH_TEMPLATE([MNT2_NFS_OPT_RDIRALOOK],
[Do lookup with readdir (nqnfs)])

AH_TEMPLATE([MNT2_NFS_OPT_PROPLIST],
[allow property list operations (ACLs over NFS)])

AH_TEMPLATE([MNT2_NFS_OPT_RDIRPLUS],
[Use Readdirplus for NFSv3])

AH_TEMPLATE([MNT2_NFS_OPT_READAHEAD],
[set read ahead])

AH_TEMPLATE([MNT2_NFS_OPT_READDIRSIZE],
[Set readdir size])

AH_TEMPLATE([MNT2_NFS_OPT_RESVPORT],
[Allocate a reserved port])

AH_TEMPLATE([MNT2_NFS_OPT_RETRANS],
[set number of request retries])

AH_TEMPLATE([MNT2_NFS_OPT_RONLY],
[read only])

AH_TEMPLATE([MNT2_NFS_OPT_RPCTIMESYNC],
[use RPC to do secure NFS time sync])

AH_TEMPLATE([MNT2_NFS_OPT_RSIZE],
[set read size])

AH_TEMPLATE([MNT2_NFS_OPT_SECURE],
[secure mount])

AH_TEMPLATE([MNT2_NFS_OPT_SNDLOCK],
[Send socket lock])

AH_TEMPLATE([MNT2_NFS_OPT_SOFT],
[soft mount (hard is default)])

AH_TEMPLATE([MNT2_NFS_OPT_SPONGY],
[spongy mount])

AH_TEMPLATE([MNT2_NFS_OPT_TIMEO],
[set initial timeout])

AH_TEMPLATE([MNT2_NFS_OPT_TCP],
[use TCP for mounts])

AH_TEMPLATE([MNT2_NFS_OPT_VER3],
[linux NFSv3])

AH_TEMPLATE([MNT2_NFS_OPT_WAITAUTH],
[Wait for authentication])

AH_TEMPLATE([MNT2_NFS_OPT_WANTAUTH],
[Wants an authenticator])

AH_TEMPLATE([MNT2_NFS_OPT_WANTRCV],
[Want receive socket lock])

AH_TEMPLATE([MNT2_NFS_OPT_WANTSND],
[Want send socket lock])

AH_TEMPLATE([MNT2_NFS_OPT_WSIZE],
[set write size])

AH_TEMPLATE([MNT2_NFS_OPT_SYMTTL],
[set symlink cache time-to-live])

AH_TEMPLATE([MNT2_NFS_OPT_PGTHRESH],
[paging threshold])

AH_TEMPLATE([MNT2_NFS_OPT_XLATECOOKIE],
[32<->64 dir cookie translation])

AH_TEMPLATE([MNT2_CDFS_OPT_DEFPERM],
[Ignore permission bits])

AH_TEMPLATE([MNT2_CDFS_OPT_NODEFPERM],
[Use on-disk permission bits])

AH_TEMPLATE([MNT2_CDFS_OPT_NOVERSION],
[Strip off extension from version string])

AH_TEMPLATE([MNT2_CDFS_OPT_RRIP],
[Use Rock Ridge Interchange Protocol (RRIP) extensions])

AH_TEMPLATE([HAVE_MNTENT_T_MNT_TIME_STRING],
[does mntent_t have mnt_time field and is of type "char *" ?])

AH_TEMPLATE([REINSTALL_SIGNAL_HANDLER],
[should signal handlers be reinstalled?])

AH_TEMPLATE([DEBUG],
[Turn off general debugging by default])

AH_TEMPLATE([DEBUG_MEM],
[Turn off memory debugging by default])

AH_TEMPLATE([PACKAGE_NAME],
[Define package name (must be defined by configure.in)])

AH_TEMPLATE([PACKAGE_VERSION],
[Define version of package (must be defined by configure.in)])

AH_TEMPLATE([PACKAGE_BUGREPORT],
[Define bug-reporting address (must be defined by configure.in)])

AH_TEMPLATE([HOST_CPU],
[Define name of host machine's cpu (eg. sparc)])

AH_TEMPLATE([HOST_ARCH],
[Define name of host machine's architecture (eg. sun4)])

AH_TEMPLATE([DISTRO_NAME],
[Define name of host OS's distribution name (eg. debian, redhat, suse, etc.)])

AH_TEMPLATE([HOST_VENDOR],
[Define name of host machine's vendor (eg. sun)])

AH_TEMPLATE([HOST_OS],
[Define name and version of host machine (eg. solaris2.5.1)])

AH_TEMPLATE([HOST_OS_NAME],
[Define only name of host machine OS (eg. solaris2)])

AH_TEMPLATE([HOST_OS_VERSION],
[Define only version of host machine (eg. 2.5.1)])

AH_TEMPLATE([HOST_HEADER_VERSION],
[Define the header version of (linux) hosts (eg. 2.2.10)])

AH_TEMPLATE([HOST_NAME],
[Define name of host])

AH_TEMPLATE([USER_NAME],
[Define user name])

AH_TEMPLATE([CONFIG_DATE],
[Define configuration date])

AH_TEMPLATE([HAVE_TRANSPORT_TYPE_TLI],
[what type of network transport type is in use?  TLI or sockets?])

AH_TEMPLATE([time_t],
[Define to `long' if <sys/types.h> doesn't define time_t])

AH_TEMPLATE([voidp],
[Define to "void *" if compiler can handle, otherwise "char *"])

AH_TEMPLATE([am_nfs_fh],
[Define a type/structure for an NFS V2 filehandle])

AH_TEMPLATE([am_nfs_fh3],
[Define a type/structure for an NFS V3 filehandle])

AH_TEMPLATE([HAVE_NFS_PROT_HEADERS],
[define if the host has NFS protocol headers in system headers])

AH_TEMPLATE([AMU_NFS_PROTOCOL_HEADER],
[define name of am-utils' NFS protocol header])

AH_TEMPLATE([nfs_args_t],
[Define a type for the nfs_args structure])

AH_TEMPLATE([NFS_FH_FIELD],
[Define the field name for the filehandle within nfs_args_t])

AH_TEMPLATE([HAVE_FHANDLE],
[Define if plain fhandle type exists])

AH_TEMPLATE([SVC_IN_ARG_TYPE],
[Define the type of the 3rd argument ('in') to svc_getargs()])

AH_TEMPLATE([XDRPROC_T_TYPE],
[Define to the type of xdr procedure type])

AH_TEMPLATE([MOUNT_TABLE_ON_FILE],
[Define if mount table is on file, undefine if in kernel])

AH_TEMPLATE([HAVE_STRUCT_MNTENT],
[Define if have struct mntent in one of the standard headers])

AH_TEMPLATE([HAVE_STRUCT_MNTTAB],
[Define if have struct mnttab in one of the standard headers])

AH_TEMPLATE([HAVE_STRUCT_NFS_ARGS],
[Define if have struct nfs_args in one of the standard nfs headers])

AH_TEMPLATE([HAVE_STRUCT_NFS_GFS_MOUNT],
[Define if have struct nfs_gfs_mount in one of the standard nfs headers])

AH_TEMPLATE([YP_ORDER_OUTORDER_TYPE],
[Type of the 3rd argument to yp_order()])

AH_TEMPLATE([RECVFROM_FROMLEN_TYPE],
[Type of the 6th argument to recvfrom()])

AH_TEMPLATE([AUTH_CREATE_GIDLIST_TYPE],
[Type of the 5rd argument to authunix_create()])

AH_TEMPLATE([MTYPE_PRINTF_TYPE],
[The string used in printf to print the mount-type field of mount(2)])

AH_TEMPLATE([MTYPE_TYPE],
[Type of the mount-type field in the mount() system call])

AH_TEMPLATE([pcfs_args_t],
[Define a type for the pcfs_args structure])

AH_TEMPLATE([autofs_args_t],
[Define a type for the autofs_args structure])

AH_TEMPLATE([cachefs_args_t],
[Define a type for the cachefs_args structure])

AH_TEMPLATE([tmpfs_args_t],
[Define a type for the tmpfs_args structure])

AH_TEMPLATE([ufs_args_t],
[Define a type for the ufs_args structure])

AH_TEMPLATE([efs_args_t],
[Define a type for the efs_args structure])

AH_TEMPLATE([xfs_args_t],
[Define a type for the xfs_args structure])

AH_TEMPLATE([lofs_args_t],
[Define a type for the lofs_args structure])

AH_TEMPLATE([cdfs_args_t],
[Define a type for the cdfs_args structure])

AH_TEMPLATE([mfs_args_t],
[Define a type for the mfs_args structure])

AH_TEMPLATE([rfs_args_t],
[Define a type for the rfs_args structure])

AH_TEMPLATE([HAVE_BAD_HASMNTOPT],
[define if have a bad version of hasmntopt()])

AH_TEMPLATE([HAVE_BAD_MEMCMP],
[define if have a bad version of memcmp()])

AH_TEMPLATE([HAVE_BAD_YP_ALL],
[define if have a bad version of yp_all()])

AH_TEMPLATE([USE_UNCONNECTED_NFS_SOCKETS],
[define if must use NFS "noconn" option])

AH_TEMPLATE([USE_CONNECTED_NFS_SOCKETS],
[define if must NOT use NFS "noconn" option])

AH_TEMPLATE([HAVE_GNU_GETOPT],
[define if your system's getopt() is GNU getopt() (are you using glibc)])

AH_TEMPLATE([HAVE_EXTERN_SYS_ERRLIST],
[does extern definition for sys_errlist[] exist?])

AH_TEMPLATE([HAVE_EXTERN_OPTARG],
[does extern definition for optarg exist?])

AH_TEMPLATE([HAVE_EXTERN_CLNT_SPCREATEERROR],
[does extern definition for clnt_spcreateerror() exist?])

AH_TEMPLATE([HAVE_EXTERN_CLNT_SPERRNO],
[does extern definition for clnt_sperrno() exist?])

AH_TEMPLATE([HAVE_EXTERN_FREE],
[does extern definition for free() exist?])

AH_TEMPLATE([HAVE_EXTERN_GET_MYADDRESS],
[does extern definition for get_myaddress() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETCCENT],
[does extern definition for getccent() (hpux) exist?])

AH_TEMPLATE([HAVE_EXTERN_GETDOMAINNAME],
[does extern definition for getdomainname() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETHOSTNAME],
[does extern definition for gethostname() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETLOGIN],
[does extern definition for getlogin() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETTABLESIZE],
[does extern definition for gettablesize() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETPAGESIZE],
[does extern definition for getpagesize() exist?])

AH_TEMPLATE([HAVE_EXTERN_HOSTS_CTL],
[does extern definition for hosts_ctl() exist?])

AH_TEMPLATE([HAVE_EXTERN_INNETGR],
[does extern definition for innetgr() exist?])

AH_TEMPLATE([HAVE_EXTERN_MKSTEMP],
[does extern definition for mkstemp() exist?])

AH_TEMPLATE([HAVE_EXTERN_SBRK],
[does extern definition for sbrk() exist?])

AH_TEMPLATE([HAVE_EXTERN_SETEUID],
[does extern definition for seteuid() exist?])

AH_TEMPLATE([HAVE_EXTERN_SETITIMER],
[does extern definition for setitimer() exist?])

AH_TEMPLATE([HAVE_EXTERN_STRCASECMP],
[does extern definition for strcasecmp() exist?])

AH_TEMPLATE([HAVE_EXTERN_STRDUP],
[does extern definition for strdup() exist?])

AH_TEMPLATE([HAVE_EXTERN_STRLCPY],
[does extern definition for strlcpy() exist?])

AH_TEMPLATE([HAVE_EXTERN_STRSTR],
[does extern definition for strstr() exist?])

AH_TEMPLATE([HAVE_EXTERN_USLEEP],
[does extern definition for usleep() exist?])

AH_TEMPLATE([HAVE_EXTERN_WAIT3],
[does extern definition for wait3() exist?])

AH_TEMPLATE([HAVE_EXTERN_VSNPRINTF],
[does extern definition for vsnprintf() exist?])

AH_TEMPLATE([HAVE_EXTERN_XDR_CALLMSG],
[does extern definition for xdr_callmsg() exist?])

AH_TEMPLATE([HAVE_EXTERN_XDR_OPAQUE_AUTH],
[does extern definition for xdr_opaque_auth() exist?])

AH_TEMPLATE([NEW_DBM_H],
[Defined to the header file containing ndbm-compatible definitions])

AH_TEMPLATE([HAVE_LIBWRAP],
[does libwrap exist?])

AH_TEMPLATE([NEED_LIBWRAP_SEVERITY_VARIABLES],
[does libwrap expect caller to define the variables allow_severity and deny_severity])

AH_TEMPLATE([HAVE_EXTERN_LDAP_ENABLE_CACHE],
[does extern definition for ldap_enable_cache() exist?])


dnl ######################################################################
dnl AC_HOST_MACROS: define HOST_CPU, HOST_VENDOR, and HOST_OS
AC_DEFUN([AMU_HOST_MACROS],
[
# these are defined already by the macro 'CANONICAL_HOST'
  AC_MSG_CHECKING([host cpu])
  AC_DEFINE_UNQUOTED(HOST_CPU, "$host_cpu")
  AC_MSG_RESULT($host_cpu)

  AC_MSG_CHECKING([vendor])
  AC_DEFINE_UNQUOTED(HOST_VENDOR, "$host_vendor")
  AC_MSG_RESULT($host_vendor)

  AC_MSG_CHECKING([host full OS name and version])
  # normalize some host OS names
  case ${host_os} in
	# linux is linux is linux, regardless of RMS.
	linux-gnu* | lignux* )	host_os=linux ;;
  esac
  AC_DEFINE_UNQUOTED(HOST_OS, "$host_os")
  AC_MSG_RESULT($host_os)

# break host_os into host_os_name and host_os_version
  AC_MSG_CHECKING([host OS name])
  host_os_name=`echo $host_os | sed 's/\..*//g'`
  # normalize some OS names
  case ${host_os_name} in
	# linux is linux is linux, regardless of RMS.
	linux-gnu* | lignux* )	host_os_name=linux ;;
  esac
  AC_DEFINE_UNQUOTED(HOST_OS_NAME, "$host_os_name")
  AC_MSG_RESULT($host_os_name)

# parse out the OS version of the host
  AC_MSG_CHECKING([host OS version])
  host_os_version=`echo $host_os | sed 's/^[[^0-9]]*//g'`
  if test -z "$host_os_version"
  then
	host_os_version=`(uname -r) 2>/dev/null` || host_os_version=unknown
  fi
  case ${host_os_version} in
	# fixes for some OS versions (solaris used to be here)
	* ) # do nothing for now
	;;
  esac
  AC_DEFINE_UNQUOTED(HOST_OS_VERSION, "$host_os_version")
  AC_MSG_RESULT($host_os_version)

# figure out host architecture (different than CPU)
  AC_MSG_CHECKING([host OS architecture])
  host_arch=`(uname -m) 2>/dev/null` || host_arch=unknown
  # normalize some names
  case ${host_arch} in
	sun4* )	host_arch=sun4 ;;
	sun3x )	host_arch=sun3 ;;
	sun )	host_arch=`(arch) 2>/dev/null` || host_arch=unknown ;;
	i?86 )	host_arch=i386 ;; # all x86 should show up as i386
  esac
  AC_DEFINE_UNQUOTED(HOST_ARCH, "$host_arch")
  AC_MSG_RESULT($host_arch)

# figure out (linux) distribution, if any
  AC_MSG_CHECKING([OS system distribution])
  ac_config_distro=`$SHELL $ac_aux_dir/config.guess.long | cut -d'-' -f4-`
  if test -z "$ac_config_distro"
  then
    ac_config_distro="none"
  fi
  AC_DEFINE_UNQUOTED(DISTRO_NAME, "$ac_config_distro")
  AC_MSG_RESULT($ac_config_distro)

# figure out host name
  AC_MSG_CHECKING([host name])
  host_name=`(hostname || uname -n) 2>/dev/null` || host_name=unknown
  AC_DEFINE_UNQUOTED(HOST_NAME, "$host_name")
  AC_MSG_RESULT($host_name)

# figure out user name
  AC_MSG_CHECKING([user name])
  if test -n "$USER"
  then
    user_name="$USER"
  else
    if test -n "$LOGNAME"
    then
      user_name="$LOGNAME"
    else
      user_name=`(whoami) 2>/dev/null` || user_name=unknown
    fi
  fi
  AC_DEFINE_UNQUOTED(USER_NAME, "$user_name")
  AC_MSG_RESULT($user_name)

# figure out configuration date
  AC_MSG_CHECKING([configuration date])
  config_date=`(date) 2>/dev/null` || config_date=unknown_date
  AC_DEFINE_UNQUOTED(CONFIG_DATE, "$config_date")
  AC_MSG_RESULT($config_date)

])
dnl ======================================================================


dnl ######################################################################
dnl ensure that linux kernel headers match running kernel
AC_DEFUN([AMU_LINUX_HEADERS],
[
# test sanity of running kernel vs. kernel headers
  AC_MSG_CHECKING("host headers version")
  case ${host_os} in
    linux )
      host_header_version="bad"
      AMU_EXPAND_RUN_STRING(
[
#include <stdio.h>
#include <linux/version.h>
],
[
if (argc > 1)
  printf("%s", UTS_RELEASE);
],
[ host_header_version=$value ],
[ echo
  AC_MSG_ERROR([cannot find UTS_RELEASE in <linux/version.h>.
  This Linux system may be misconfigured or unconfigured!])
])
	;;
	* ) host_header_version=$host_os_version ;;
  esac
  AC_DEFINE_UNQUOTED(HOST_HEADER_VERSION, "$host_header_version")
  AC_MSG_RESULT($host_header_version)

  case ${host_os} in
    linux )
	if test "$host_os_version" != $host_header_version
	then
		AC_MSG_WARN([Linux kernel $host_os_version mismatch with $host_header_version headers!])
	fi
    ;;
esac
dnl cache these two for debugging purposes
ac_cv_os_version=$host_os_version
ac_cv_header_version=$host_header_version
])
dnl ======================================================================


dnl ######################################################################
dnl check if a local configuration file exists
AC_DEFUN([AMU_LOCALCONFIG],
[AC_MSG_CHECKING(a local configuration file)
if test -f localconfig.h
then
  AC_DEFINE(HAVE_LOCALCONFIG_H)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl an M4 macro to include a list of common headers being used everywhere
define(AMU_MOUNT_HEADERS,
[
#include "${srcdir}/include/mount_headers1.h"
#include AMU_NFS_PROTOCOL_HEADER
#include "${srcdir}/include/mount_headers2.h"

$1
]
)
dnl ======================================================================


dnl ######################################################################
dnl Which options to add to CFLAGS for compilation?
dnl NOTE: this is only for final compiltions, not for configure tests)
AC_DEFUN([AMU_OPT_AMU_CFLAGS],
[AC_MSG_CHECKING(for additional C option compilation flags)
AC_ARG_ENABLE(am-cflags,
AC_HELP_STRING([--enable-am-cflags=ARG],
		[compile package with ARG additional C flags]),
[
if test "$enableval" = "" || test "$enableval" = "yes" || test "$enableval" = "no"; then
  AC_MSG_ERROR(am-cflags must be supplied if option is used)
fi
# user supplied a cflags option to configure
AMU_CFLAGS="$enableval"
AC_SUBST(AMU_CFLAGS)
AC_MSG_RESULT($enableval)
], [
  # default is to have no additional C flags
  AMU_CFLAGS=""
  AC_SUBST(AMU_CFLAGS)
  AC_MSG_RESULT(none)
])
])
dnl ======================================================================


dnl ######################################################################
dnl Initial settings for CPPFLAGS (-I options)
dnl NOTE: this is for configuration as well as compilations!
AC_DEFUN([AMU_OPT_CPPFLAGS],
[AC_MSG_CHECKING(for configuration/compilation (-I) preprocessor flags)
AC_ARG_ENABLE(cppflags,
AC_HELP_STRING([--enable-cppflags=ARG],
	[configure/compile with ARG (-I) preprocessor flags]),
[
if test "$enableval" = "" || test "$enableval" = "yes" || test "$enableval" = "no"; then
  AC_MSG_ERROR(cppflags must be supplied if option is used)
fi
# use supplied options
CPPFLAGS="$CPPFLAGS $enableval"
export CPPFLAGS
AC_MSG_RESULT($enableval)
], [
  # default is to have no additional flags
  AC_MSG_RESULT(none)
])
])
dnl ======================================================================


dnl ######################################################################
dnl Debugging: "yes" means general, "mem" means general and memory debugging,
dnl and "no" means none.
AC_DEFUN([AMU_OPT_DEBUG],
[AC_MSG_CHECKING(for debugging options)
AC_ARG_ENABLE(debug,
AC_HELP_STRING([--enable-debug=ARG],[enable debugging (yes/mem/no)]),
[
if test "$enableval" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(DEBUG)
  ac_cv_opt_debug=yes
elif test "$enableval" = mem; then
  AC_MSG_RESULT(mem)
  AC_DEFINE(DEBUG)
  AC_DEFINE(DEBUG_MEM)
  AC_CHECK_FUNC(malloc_verify,,AC_CHECK_LIB(mapmalloc, malloc_verify))
  AC_CHECK_FUNC(mallinfo,,AC_CHECK_LIB(malloc, mallinfo))
  ac_cv_opt_debug=mem
else
  AC_MSG_RESULT(no)
  ac_cv_opt_debug=no
fi
],
[
  # default is no debugging
  AC_MSG_RESULT(no)
])
])
dnl ======================================================================


dnl ######################################################################
dnl Initial settings for LDFLAGS (-L options)
dnl NOTE: this is for configuration as well as compilations!
AC_DEFUN([AMU_OPT_LDFLAGS],
[AC_MSG_CHECKING(for configuration/compilation (-L) library flags)
AC_ARG_ENABLE(ldflags,
AC_HELP_STRING([--enable-ldflags=ARG],
		[configure/compile with ARG (-L) library flags]),
[
if test "$enableval" = "" || test "$enableval" = "yes" || test "$enableval" = "no"; then
  AC_MSG_ERROR(ldflags must be supplied if option is used)
fi
# use supplied options
LDFLAGS="$LDFLAGS $enableval"
export LDFLAGS
AC_MSG_RESULT($enableval)
], [
  # default is to have no additional flags
  AC_MSG_RESULT(none)
])
])
dnl ======================================================================


dnl ######################################################################
dnl Initial settings for LIBS (-l options)
dnl NOTE: this is for configuration as well as compilations!
AC_DEFUN([AMU_OPT_LIBS],
[AC_MSG_CHECKING(for configuration/compilation (-l) library flags)
AC_ARG_ENABLE(libs,
AC_HELP_STRING([--enable-libs=ARG],
		[configure/compile with ARG (-l) library flags]),
[
if test "$enableval" = "" || test "$enableval" = "yes" || test "$enableval" = "no"; then
  AC_MSG_ERROR(libs must be supplied if option is used)
fi
# use supplied options
LIBS="$LIBS $enableval"
export LIBS
AC_MSG_RESULT($enableval)
], [
  # default is to have no additional flags
  AC_MSG_RESULT(none)
])
])
dnl ======================================================================


dnl ######################################################################
dnl Specify additional compile options based on the OS and the compiler
AC_DEFUN([AMU_OS_CFLAGS],
[
AC_CACHE_CHECK(additional compiler flags,
ac_cv_os_cflags,
[
case "${host_os}" in
	irix6* )
		case "${CC}" in
			cc )
				# do not use 64-bit compiler
				ac_cv_os_cflags="-n32 -mips3 -Wl,-woff,84"
				;;
		esac
		;;
	osf[[1-3]]* )
		# get the right version of struct sockaddr
		case "${CC}" in
			cc )
				ac_cv_os_cflags="-std -D_SOCKADDR_LEN -D_NO_PROTO"
				;;
			* )
				ac_cv_os_cflags="-D_SOCKADDR_LEN -D_NO_PROTO"
				;;
		esac
		;;
	osf* )
		# get the right version of struct sockaddr
		case "${CC}" in
			cc )
				ac_cv_os_cflags="-std -D_SOCKADDR_LEN"
				;;
			* )
				ac_cv_os_cflags="-D_SOCKADDR_LEN"
				;;
		esac
		;;
	aix[[1-3]]* )
		ac_cv_os_cflags="" ;;
	aix4.[[0-2]]* )
		# turn on additional headers
		ac_cv_os_cflags="-D_XOPEN_EXTENDED_SOURCE"
		;;
	aix5.3* )
		# avoid circular dependencies in yp headers, and more
		ac_cv_os_cflags="-DHAVE_BAD_HEADERS -D_XOPEN_EXTENDED_SOURCE -D_USE_IRS -D_MSGQSUPPORT"
		;;
	aix* )
		# avoid circular dependencies in yp headers
		ac_cv_os_cflags="-DHAVE_BAD_HEADERS -D_XOPEN_EXTENDED_SOURCE -D_USE_IRS"
		;;
	OFF-sunos4* )
		# make sure passing whole structures is handled in gcc
		case "${CC}" in
			gcc )
				ac_cv_os_cflags="-fpcc-struct-return"
				;;
		esac
		;;
	sunos[[34]]* | solaris1* | solaris2.[[0-5]] | sunos5.[[0-5]] | solaris2.5.* | sunos5.5.* )
		ac_cv_os_cflags="" ;;
	solaris2* | sunos5* )
		# turn on 64-bit file offset interface
		case "${CC}" in
			* )
				ac_cv_os_cflags="-D_LARGEFILE64_SOURCE"
				;;
		esac
		;;
	hpux* )
		# use Ansi compiler on HPUX
		case "${CC}" in
			cc )
				ac_cv_os_cflags="-Ae"
				;;
		esac
		;;
	darwin* | rhapsody* )
		ac_cv_os_cflags="-D_P1003_1B_VISIBLE"
		;;
	* )
		ac_cv_os_cflags=""
		;;
esac
])
AMU_CFLAGS="$AMU_CFLAGS $ac_cv_os_cflags"
])
dnl ======================================================================


dnl ######################################################################
dnl Specify additional cpp options based on the OS and the compiler
AC_DEFUN([AMU_OS_CPPFLAGS],
[
AC_CACHE_CHECK(additional preprocessor flags,
ac_cv_os_cppflags,
[
case "${host_os}" in
# off for now, posix may be a broken thing for nextstep3...
#	nextstep* )
#		ac_cv_os_cppflags="-D_POSIX_SOURCE"
#		;;
	* )	ac_cv_os_cppflags="" ;;
esac
])
CPPFLAGS="$CPPFLAGS $ac_cv_os_cppflags"
])
dnl ======================================================================


dnl ######################################################################
dnl Specify additional linker options based on the OS and the compiler
AC_DEFUN([AMU_OS_LDFLAGS],
[
AC_CACHE_CHECK(additional linker flags,
ac_cv_os_ldflags,
[
case "${host_os}" in
	solaris2.7* | sunos5.7* )
		# find LDAP: off until Sun includes ldap headers.
		case "${CC}" in
			* )
				#ac_cv_os_ldflags="-L/usr/lib/fn"
				;;
		esac
		;;
	* )	ac_cv_os_ldflags="" ;;
esac
])
LDFLAGS="$LDFLAGS $ac_cv_os_ldflags"
])
dnl ======================================================================


dnl ######################################################################
dnl Bugreport name
AC_DEFUN([AMU_PACKAGE_BUGREPORT],
[AC_MSG_CHECKING(bug-reporting address)
AC_DEFINE_UNQUOTED(PACKAGE_BUGREPORT, "$1")
AC_MSG_RESULT(\"$1\")
])
dnl ======================================================================


dnl ######################################################################
dnl Package name
AC_DEFUN([AMU_PACKAGE_NAME],
[AC_MSG_CHECKING(package name)
AC_DEFINE_UNQUOTED(PACKAGE_NAME, "$1")
AC_MSG_RESULT(\"$1\")
])
dnl ======================================================================


dnl ######################################################################
dnl Version of package
AC_DEFUN([AMU_PACKAGE_VERSION],
[AC_MSG_CHECKING(version of package)
AC_DEFINE_UNQUOTED(PACKAGE_VERSION, "$1")
AC_MSG_RESULT(\"$1\")
])
dnl ======================================================================


dnl ######################################################################
dnl AC_SAVE_STATE: save confdefs.h onto dbgcf.h and write $ac_cv_* cache
dnl variables that are known so far.
define(AMU_SAVE_STATE,
AC_MSG_NOTICE(*** SAVING CONFIGURE STATE ***)
if test -f confdefs.h
then
 cp confdefs.h dbgcf.h
fi
[AC_CACHE_SAVE]
)
dnl ======================================================================


dnl ######################################################################
dnl Find the name of the nfs filehandle field in nfs_args_t.
AC_DEFUN([AMU_STRUCT_FIELD_NFS_FH],
[
dnl make sure this is called before macros which depend on it
AC_BEFORE([$0], [AC_TYPE_NFS_FH])
AC_CACHE_CHECK(for the name of the nfs filehandle field in nfs_args_t,
ac_cv_struct_field_nfs_fh,
[
# set to a default value
ac_cv_struct_field_nfs_fh=notfound
# look for name "fh" (most systems)
if test "$ac_cv_struct_field_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfs_args_t nat;
  char *cp = (char *) &(nat.fh);
], ac_cv_struct_field_nfs_fh=fh, ac_cv_struct_field_nfs_fh=notfound)
fi

# look for name "root" (for example Linux)
if test "$ac_cv_struct_field_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfs_args_t nat;
  char *cp = (char *) &(nat.root);
], ac_cv_struct_field_nfs_fh=root, ac_cv_struct_field_nfs_fh=notfound)
fi
])
if test "$ac_cv_struct_field_nfs_fh" != notfound
then
  AC_DEFINE_UNQUOTED(NFS_FH_FIELD, $ac_cv_struct_field_nfs_fh)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find if struct mntent exists anywhere in mount.h or mntent.h headers
AC_DEFUN([AMU_STRUCT_MNTENT],
[
AC_CACHE_CHECK(for struct mntent,
ac_cv_have_struct_mntent,
[
# try to compile a program which may have a definition for the structure
AC_TRY_COMPILE(
AMU_MOUNT_HEADERS
,
[
struct mntent mt;
], ac_cv_have_struct_mntent=yes, ac_cv_have_struct_mntent=no)
])
if test "$ac_cv_have_struct_mntent" = yes
then
  AC_DEFINE(HAVE_STRUCT_MNTENT)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find if struct mnttab exists anywhere in mount.h or mnttab.h headers
AC_DEFUN([AMU_STRUCT_MNTTAB],
[
AC_CACHE_CHECK(for struct mnttab,
ac_cv_have_struct_mnttab,
[
# try to compile a program which may have a definition for the structure
AC_TRY_COMPILE(
AMU_MOUNT_HEADERS
,
[
struct mnttab mt;
], ac_cv_have_struct_mnttab=yes, ac_cv_have_struct_mnttab=no)
])
if test "$ac_cv_have_struct_mnttab" = yes
then
  AC_DEFINE(HAVE_STRUCT_MNTTAB)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find if struct nfs_args exists anywhere in typical headers
AC_DEFUN([AMU_STRUCT_NFS_ARGS],
[
dnl make sure this is called before [AC_TYPE_NFS_FH]
AC_BEFORE([$0], [AC_TYPE_NFS_FH])
AC_BEFORE([$0], [AC_STRUCT_FIELD_NFS_FH])
AC_CACHE_CHECK(for struct nfs_args,
ac_cv_have_struct_nfs_args,
[
# try to compile a program which may have a definition for the structure
# assume not found
ac_cv_have_struct_nfs_args=notfound

# look for "struct irix5_nfs_args" (specially set in conf/nfs_prot/)
if test "$ac_cv_have_struct_nfs_args" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct irix5_nfs_args na;
], ac_cv_have_struct_nfs_args="struct irix5_nfs_args", ac_cv_have_struct_nfs_args=notfound)
fi

# look for "struct aix5_nfs_args" (specially set in conf/nfs_prot/)
if test "$ac_cv_have_struct_nfs_args" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct aix5_nfs_args na;
], ac_cv_have_struct_nfs_args="struct aix5_nfs_args", ac_cv_have_struct_nfs_args=notfound)
fi

# look for "struct aix4_nfs_args" (specially set in conf/nfs_prot/)
if test "$ac_cv_have_struct_nfs_args" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct aix4_nfs_args na;
], ac_cv_have_struct_nfs_args="struct aix4_nfs_args", ac_cv_have_struct_nfs_args=notfound)
fi

# look for "struct nfs_args"
if test "$ac_cv_have_struct_nfs_args" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct nfs_args na;
], ac_cv_have_struct_nfs_args="struct nfs_args", ac_cv_have_struct_nfs_args=notfound)
fi

])

if test "$ac_cv_have_struct_nfs_args" != notfound
then
  AC_DEFINE(HAVE_STRUCT_NFS_ARGS)
  AC_DEFINE_UNQUOTED(nfs_args_t, $ac_cv_have_struct_nfs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the structure of an nfs filehandle.
dnl if found, defined am_nfs_fh to it, else leave it undefined.
dnl THE ORDER OF LOOKUPS IN THIS FILE IS VERY IMPORTANT!!!
AC_DEFUN([AMU_STRUCT_NFS_FH],
[
AC_CACHE_CHECK(for type/structure of NFS V2 filehandle,
ac_cv_struct_nfs_fh,
[
# try to compile a program which may have a definition for the type
dnl need a series of compilations, which will test out every possible type
dnl such as struct nfs_fh, fhandle_t, nfsv2fh_t, etc.
# set to a default value
ac_cv_struct_nfs_fh=notfound

# look for "nfs_fh"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfs_fh nh;
], ac_cv_struct_nfs_fh="nfs_fh", ac_cv_struct_nfs_fh=notfound)
fi

# look for "struct nfs_fh"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct nfs_fh nh;
], ac_cv_struct_nfs_fh="struct nfs_fh", ac_cv_struct_nfs_fh=notfound)
fi

# look for "struct nfssvcfh"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ struct nfssvcfh nh;
], ac_cv_struct_nfs_fh="struct nfssvcfh", ac_cv_struct_nfs_fh=notfound)
fi

# look for "nfsv2fh_t"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ nfsv2fh_t nh;
], ac_cv_struct_nfs_fh="nfsv2fh_t", ac_cv_struct_nfs_fh=notfound)
fi

# look for "fhandle_t"
if test "$ac_cv_struct_nfs_fh" = notfound
then
AC_TRY_COMPILE_NFS(
[ fhandle_t nh;
], ac_cv_struct_nfs_fh="fhandle_t", ac_cv_struct_nfs_fh=notfound)
fi

])

if test "$ac_cv_struct_nfs_fh" != notfound
then
  AC_DEFINE_UNQUOTED(am_nfs_fh, $ac_cv_struct_nfs_fh)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find if struct nfs_gfs_mount exists anywhere in typical headers
AC_DEFUN([AMU_STRUCT_NFS_GFS_MOUNT],
[
dnl make sure this is called before [AC_TYPE_NFS_FH]
AC_BEFORE([$0], [AC_TYPE_NFS_FH])
AC_BEFORE([$0], [AC_STRUCT_FIELD_NFS_FH])
AC_CACHE_CHECK(for struct nfs_gfs_mount,
ac_cv_have_struct_nfs_gfs_mount,
[
# try to compile a program which may have a definition for the structure
AC_TRY_COMPILE_NFS(
[ struct nfs_gfs_mount ngm;
], ac_cv_have_struct_nfs_gfs_mount=yes, ac_cv_have_struct_nfs_gfs_mount=no)
])
if test "$ac_cv_have_struct_nfs_gfs_mount" = yes
then
  AC_DEFINE(HAVE_STRUCT_NFS_GFS_MOUNT)
  AC_DEFINE(nfs_args_t, struct nfs_gfs_mount)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Compile a program with <any>FS headers to try and find a feature.
dnl The headers part are fixed.  Only three arguments are allowed:
dnl [$1] is the program to compile (2nd arg to AC_TRY_COMPILE)
dnl [$2] action to take if the program compiled (3rd arg to AC_TRY_COMPILE)
dnl [$3] action to take if program did not compile (4rd arg to AC_TRY_COMPILE)
AC_DEFUN([AC_TRY_COMPILE_ANYFS],
[# try to compile a program which may have a definition for a structure
AC_TRY_COMPILE(
[
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif /* HAVE_SYS_ERRNO_H */
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else /* not TIME_WITH_SYS_TIME */
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else /* not HAVE_SYS_TIME_H */
#  include <time.h>
# endif /* not HAVE_SYS_TIME_H */
#endif /* not TIME_WITH_SYS_TIME */

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#ifdef HAVE_SYS_TIUSER_H
# include <sys/tiuser.h>
#endif /* HAVE_SYS_TIUSER_H */

#ifdef HAVE_SYS_MOUNT_H
# ifndef NFSCLIENT
#  define NFSCLIENT
# endif /* not NFSCLIENT */
# ifndef PCFS
#  define PCFS
# endif /* not PCFS */
# ifndef LOFS
#  define LOFS
# endif /* not LOFS */
# ifndef RFS
#  define RFS
# endif /* not RFS */
# ifndef MSDOSFS
#  define MSDOSFS
# endif /* not MSDOSFS */
# ifndef MFS
#  define MFS 1
# endif /* not MFS */
# ifndef CD9660
#  define CD9660
# endif /* not CD9660 */
# ifndef NFS
#  define NFS
# endif /* not NFS */
# include <sys/mount.h>
#endif /* HAVE_SYS_MOUNT_H */

#ifdef HAVE_SYS_VMOUNT_H
# include <sys/vmount.h>
#endif /* HAVE_SYS_VMOUNT_H */

/*
 * There is no point in including this on a glibc2 system
 * we're only asking for trouble
 */
#if defined HAVE_LINUX_FS_H && (!defined __GLIBC__ || __GLIBC__ < 2)
/*
 * There's a conflict of definitions on redhat alpha linux between
 * <netinet/in.h> and <linux/fs.h>.
 * Also a conflict in definitions of ntohl/htonl in RH-5.1 sparc64
 * between <netinet/in.h> and <linux/byteorder/generic.h> (2.1 kernels).
 */
# ifdef HAVE_SOCKETBITS_H
#  define _LINUX_SOCKET_H
#  undef BLKFLSBUF
#  undef BLKGETSIZE
#  undef BLKRAGET
#  undef BLKRASET
#  undef BLKROGET
#  undef BLKROSET
#  undef BLKRRPART
#  undef MS_MGC_VAL
#  undef MS_RMT_MASK
# endif /* HAVE_SOCKETBITS_H */
# ifdef HAVE_LINUX_POSIX_TYPES_H
#  include <linux/posix_types.h>
# endif /* HAVE_LINUX_POSIX_TYPES_H */
# ifndef _LINUX_BYTEORDER_GENERIC_H
#  define _LINUX_BYTEORDER_GENERIC_H
# endif /* _LINUX_BYTEORDER_GENERIC_H */
# ifndef _LINUX_STRING_H_
#  define _LINUX_STRING_H_
# endif /* not _LINUX_STRING_H_ */
# ifdef HAVE_LINUX_KDEV_T_H
#  define __KERNEL__
#  include <linux/kdev_t.h>
#  undef __KERNEL__
# endif /* HAVE_LINUX_KDEV_T_H */
# ifdef HAVE_LINUX_LIST_H
#  define __KERNEL__
#  include <linux/list.h>
#  undef __KERNEL__
# endif /* HAVE_LINUX_LIST_H */
# include <linux/fs.h>
#endif /* HAVE_LINUX_FS_H && (!__GLIBC__ || __GLIBC__ < 2) */

#ifdef HAVE_SYS_FS_AUTOFS_H
# include <sys/fs/autofs.h>
#endif /* HAVE_SYS_FS_AUTOFS_H */
#ifdef HAVE_SYS_FS_CACHEFS_FS_H
# include <sys/fs/cachefs_fs.h>
#endif /* HAVE_SYS_FS_CACHEFS_FS_H */

#ifdef HAVE_SYS_FS_PC_FS_H
# include <sys/fs/pc_fs.h>
#endif /* HAVE_SYS_FS_PC_FS_H */
#ifdef HAVE_MSDOSFS_MSDOSFSMOUNT_H
# include <msdosfs/msdosfsmount.h>
#endif /* HAVE_MSDOSFS_MSDOSFSMOUNT_H */
#ifdef HAVE_FS_MSDOSFS_MSDOSFSMOUNT_H
# include <fs/msdosfs/msdosfsmount.h>
#endif /* HAVE_FS_MSDOSFS_MSDOSFSMOUNT_H */

#ifdef HAVE_SYS_FS_TMP_H
# include <sys/fs/tmp.h>
#endif /* HAVE_SYS_FS_TMP_H */

#ifdef HAVE_UFS_UFS_MOUNT_H
# include <ufs/ufs_mount.h>
#endif /* HAVE_UFS_UFS_MOUNT_H */
#ifdef HAVE_UFS_UFS_UFSMOUNT_H
# ifndef MAXQUOTAS
#  define MAXQUOTAS     2
# endif /* not MAXQUOTAS */
struct netexport { int this_is_SO_wrong; }; /* for bsdi-2.1 */
/* netbsd-1.4 does't protect <ufs/ufs/ufsmount.h> */
# ifndef _UFS_UFS_UFSMOUNT_H
#  include <ufs/ufs/ufsmount.h>
#  define _UFS_UFS_UFSMOUNT_H
# endif /* not _UFS_UFS_UFSMOUNT_H */
#endif /* HAVE_UFS_UFS_UFSMOUNT_H */
#ifdef HAVE_SYS_FS_UFS_MOUNT_H
# include <sys/fs/ufs_mount.h>
#endif /* HAVE_SYS_FS_UFS_MOUNT_H */
#ifdef HAVE_SYS_FS_EFS_CLNT_H
# include <sys/fs/efs_clnt.h>
#endif /* HAVE_SYS_FS_EFS_CLNT_H */
#ifdef HAVE_SYS_FS_XFS_CLNT_H
# include <sys/fs/xfs_clnt.h>
#endif /* HAVE_SYS_FS_XFS_CLNT_H */

#ifdef HAVE_CDFS_CDFS_MOUNT_H
# include <cdfs/cdfs_mount.h>
#endif /* HAVE_CDFS_CDFS_MOUNT_H */
#ifdef HAVE_HSFS_HSFS_H
# include <hsfs/hsfs.h>
#endif /* HAVE_HSFS_HSFS_H */
#ifdef HAVE_CDFS_CDFSMOUNT_H
# include <cdfs/cdfsmount.h>
#endif /* HAVE_CDFS_CDFSMOUNT_H */
#ifdef HAVE_ISOFS_CD9660_CD9660_MOUNT_H
# include <isofs/cd9660/cd9660_mount.h>
#endif /* HAVE_ISOFS_CD9660_CD9660_MOUNT_H */
], [$1], [$2], [$3])
])
dnl ======================================================================


dnl ######################################################################
dnl Compile a program with NFS headers to try and find a feature.
dnl The headers part are fixed.  Only three arguments are allowed:
dnl [$1] is the program to compile (2nd arg to AC_TRY_COMPILE)
dnl [$2] action to take if the program compiled (3rd arg to AC_TRY_COMPILE)
dnl [$3] action to take if program did not compile (4rd arg to AC_TRY_COMPILE)
AC_DEFUN([AC_TRY_COMPILE_NFS],
[# try to compile a program which may have a definition for a structure
AC_TRY_COMPILE(
AMU_MOUNT_HEADERS
, [$1], [$2], [$3])
])
dnl ======================================================================


dnl ######################################################################
dnl Compile a program with RPC headers to try and find a feature.
dnl The headers part are fixed.  Only three arguments are allowed:
dnl [$1] is the program to compile (2nd arg to AC_TRY_COMPILE)
dnl [$2] action to take if the program compiled (3rd arg to AC_TRY_COMPILE)
dnl [$3] action to take if program did not compile (4rd arg to AC_TRY_COMPILE)
AC_DEFUN([AC_TRY_COMPILE_RPC],
[# try to compile a program which may have a definition for a structure
AC_TRY_COMPILE(
[
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_RPC_RPC_H
# include <rpc/rpc.h>
#endif /* HAVE_RPC_RPC_H */
/* Prevent multiple inclusion on Ultrix 4 */
#if defined(HAVE_RPC_XDR_H) && !defined(__XDR_HEADER__)
# include <rpc/xdr.h>
#endif /* defined(HAVE_RPC_XDR_H) && !defined(__XDR_HEADER__) */
], [$1], [$2], [$3])
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct type for the 5th argument to authunix_create()
AC_DEFUN([AMU_TYPE_AUTH_CREATE_GIDLIST],
[
AC_CACHE_CHECK(argument type of 5rd argument to authunix_create(),
ac_cv_auth_create_gidlist,
[
# select the correct type
case "${host_os_name}" in
	sunos[[34]]* | bsdi2* | sysv4* | hpux10.10 | ultrix* | aix4* )
		ac_cv_auth_create_gidlist="int" ;;
	* )
		ac_cv_auth_create_gidlist="gid_t" ;;
esac
])
AC_DEFINE_UNQUOTED(AUTH_CREATE_GIDLIST_TYPE, $ac_cv_auth_create_gidlist)
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for AUTOFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_AUTOFS_ARGS],
[
AC_CACHE_CHECK(for structure type of autofs mount(2) arguments,
ac_cv_type_autofs_args,
[
# set to a default value
ac_cv_type_autofs_args=notfound

# look for "struct auto_args"
if test "$ac_cv_type_autofs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct auto_args a;
], ac_cv_type_autofs_args="struct auto_args", ac_cv_type_autofs_args=notfound)
fi

# look for "struct autofs_args"
if test "$ac_cv_type_autofs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct autofs_args a;
], ac_cv_type_autofs_args="struct autofs_args", ac_cv_type_autofs_args=notfound)
fi

])
if test "$ac_cv_type_autofs_args" != notfound
then
  AC_DEFINE_UNQUOTED(autofs_args_t, $ac_cv_type_autofs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for CACHEFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_CACHEFS_ARGS],
[
AC_CACHE_CHECK(for structure type of cachefs mount(2) arguments,
ac_cv_type_cachefs_args,
[
# set to a default value
ac_cv_type_cachefs_args=notfound
# look for "struct cachefs_mountargs"
if test "$ac_cv_type_cachefs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct cachefs_mountargs a;
], ac_cv_type_cachefs_args="struct cachefs_mountargs", ac_cv_type_cachefs_args=notfound)
fi
])
if test "$ac_cv_type_cachefs_args" != notfound
then
  AC_DEFINE_UNQUOTED(cachefs_args_t, $ac_cv_type_cachefs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for CDFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_CDFS_ARGS],
[
AC_CACHE_CHECK(for structure type of cdfs mount(2) arguments,
ac_cv_type_cdfs_args,
[
# set to a default value
ac_cv_type_cdfs_args=notfound

# look for "struct iso_args"
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct iso_args a;
], ac_cv_type_cdfs_args="struct iso_args", ac_cv_type_cdfs_args=notfound)
fi

# look for "struct iso9660_args"
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct iso9660_args a;
], ac_cv_type_cdfs_args="struct iso9660_args", ac_cv_type_cdfs_args=notfound)
fi

# look for "struct cdfs_args"
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct cdfs_args a;
], ac_cv_type_cdfs_args="struct cdfs_args", ac_cv_type_cdfs_args=notfound)
fi

# look for "struct hsfs_args"
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct hsfs_args a;
], ac_cv_type_cdfs_args="struct hsfs_args", ac_cv_type_cdfs_args=notfound)
fi

# look for "struct iso_specific" (ultrix)
if test "$ac_cv_type_cdfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct iso_specific a;
], ac_cv_type_cdfs_args="struct iso_specific", ac_cv_type_cdfs_args=notfound)
fi

])
if test "$ac_cv_type_cdfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(cdfs_args_t, $ac_cv_type_cdfs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for EFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_EFS_ARGS],
[
AC_CACHE_CHECK(for structure type of efs mount(2) arguments,
ac_cv_type_efs_args,
[
# set to a default value
ac_cv_type_efs_args=notfound

# look for "struct efs_args"
if test "$ac_cv_type_efs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct efs_args a;
], ac_cv_type_efs_args="struct efs_args", ac_cv_type_efs_args=notfound)
fi

])
if test "$ac_cv_type_efs_args" != notfound
then
  AC_DEFINE_UNQUOTED(efs_args_t, $ac_cv_type_efs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for LOFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_LOFS_ARGS],
[
AC_CACHE_CHECK(for structure type of lofs mount(2) arguments,
ac_cv_type_lofs_args,
[
# set to a default value
ac_cv_type_lofs_args=notfound
# look for "struct lofs_args"
if test "$ac_cv_type_lofs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct lofs_args a;
], ac_cv_type_lofs_args="struct lofs_args", ac_cv_type_lofs_args=notfound)
fi
# look for "struct lo_args"
if test "$ac_cv_type_lofs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct lo_args a;
], ac_cv_type_lofs_args="struct lo_args", ac_cv_type_lofs_args=notfound)
fi
])
if test "$ac_cv_type_lofs_args" != notfound
then
  AC_DEFINE_UNQUOTED(lofs_args_t, $ac_cv_type_lofs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for MFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_MFS_ARGS],
[
AC_CACHE_CHECK(for structure type of mfs mount(2) arguments,
ac_cv_type_mfs_args,
[
# set to a default value
ac_cv_type_mfs_args=notfound
# look for "struct mfs_args"
if test "$ac_cv_type_mfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct mfs_args a;
], ac_cv_type_mfs_args="struct mfs_args", ac_cv_type_mfs_args=notfound)
fi
])
if test "$ac_cv_type_mfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(mfs_args_t, $ac_cv_type_mfs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for PC/FS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_PCFS_ARGS],
[
AC_CACHE_CHECK(for structure type of pcfs mount(2) arguments,
ac_cv_type_pcfs_args,
[
# set to a default value
ac_cv_type_pcfs_args=notfound

# look for "struct msdos_args"
if test "$ac_cv_type_pcfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct msdos_args a;
], ac_cv_type_pcfs_args="struct msdos_args", ac_cv_type_pcfs_args=notfound)
fi

# look for "struct pc_args"
if test "$ac_cv_type_pcfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct pc_args a;
], ac_cv_type_pcfs_args="struct pc_args", ac_cv_type_pcfs_args=notfound)
fi

# look for "struct pcfs_args"
if test "$ac_cv_type_pcfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct pcfs_args a;
], ac_cv_type_pcfs_args="struct pcfs_args", ac_cv_type_pcfs_args=notfound)
fi

# look for "struct msdosfs_args"
if test "$ac_cv_type_pcfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct msdosfs_args a;
], ac_cv_type_pcfs_args="struct msdosfs_args", ac_cv_type_pcfs_args=notfound)
fi

])

if test "$ac_cv_type_pcfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(pcfs_args_t, $ac_cv_type_pcfs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct type for the 6th argument to recvfrom()
AC_DEFUN([AMU_TYPE_RECVFROM_FROMLEN],
[
AC_CACHE_CHECK(non-pointer type of 6th (fromlen) argument to recvfrom(),
ac_cv_recvfrom_fromlen,
[
# select the correct type
case "${host_os}" in
	aix[[1-3]]* )
		ac_cv_recvfrom_fromlen="int" ;;
	aix* )
		ac_cv_recvfrom_fromlen="size_t" ;;
	* )
		ac_cv_recvfrom_fromlen="int" ;;
esac
])
AC_DEFINE_UNQUOTED(RECVFROM_FROMLEN_TYPE, $ac_cv_recvfrom_fromlen)
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for RFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_RFS_ARGS],
[
AC_CACHE_CHECK(for structure type of rfs mount(2) arguments,
ac_cv_type_rfs_args,
[
# set to a default value
ac_cv_type_rfs_args=notfound
# look for "struct rfs_args"
if test "$ac_cv_type_rfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct rfs_args a;
], ac_cv_type_rfs_args="struct rfs_args", ac_cv_type_rfs_args=notfound)
fi
])
if test "$ac_cv_type_rfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(rfs_args_t, $ac_cv_type_rfs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the type of the 3rd argument (in) to svc_sendreply() call
AC_DEFUN([AMU_TYPE_SVC_IN_ARG],
[
AC_CACHE_CHECK(for type of 3rd arg ('in') arg to svc_sendreply(),
ac_cv_type_svc_in_arg,
[
# try to compile a program which may have a definition for the type
dnl need a series of compilations, which will test out every possible type
dnl such as caddr_t, char *, etc.
# set to a default value
ac_cv_type_svc_in_arg=notfound
# look for "caddr_t"
if test "$ac_cv_type_svc_in_arg" = notfound
then
AC_TRY_COMPILE_RPC(
[ SVCXPRT *SX;
  xdrproc_t xp;
  caddr_t p;
  svc_sendreply(SX, xp, p);
], ac_cv_type_svc_in_arg="caddr_t", ac_cv_type_svc_in_arg=notfound)
fi
# look for "char *"
if test "$ac_cv_type_svc_in_arg" = notfound
then
AC_TRY_COMPILE_RPC(
[ SVCXPRT *SX;
  xdrproc_t xp;
  char *p;
  svc_sendreply(SX, xp, p);
], ac_cv_type_svc_in_arg="char *", ac_cv_type_svc_in_arg=notfound)
fi
])
if test "$ac_cv_type_svc_in_arg" != notfound
then
  AC_DEFINE_UNQUOTED(SVC_IN_ARG_TYPE, $ac_cv_type_svc_in_arg)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check for type of time_t (usually in <sys/types.h>)
AC_DEFUN([AMU_TYPE_TIME_T],
[AC_CHECK_TYPE(time_t, long)])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for TMPFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_TMPFS_ARGS],
[
AC_CACHE_CHECK(for structure type of tmpfs mount(2) arguments,
ac_cv_type_tmpfs_args,
[
# set to a default value
ac_cv_type_tmpfs_args=notfound
# look for "struct tmpfs_args"
if test "$ac_cv_type_tmpfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct tmpfs_args a;
], ac_cv_type_tmpfs_args="struct tmpfs_args", ac_cv_type_tmpfs_args=notfound)
fi
])
if test "$ac_cv_type_tmpfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(tmpfs_args_t, $ac_cv_type_tmpfs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for UFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_UFS_ARGS],
[
AC_CACHE_CHECK(for structure type of ufs mount(2) arguments,
ac_cv_type_ufs_args,
[
# set to a default value
ac_cv_type_ufs_args=notfound

# look for "struct ufs_args"
if test "$ac_cv_type_ufs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct ufs_args a;
], ac_cv_type_ufs_args="struct ufs_args", ac_cv_type_ufs_args=notfound)
fi

# look for "struct efs_args" (irix)
if test "$ac_cv_type_ufs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct efs_args a;
], ac_cv_type_ufs_args="struct efs_args", ac_cv_type_ufs_args=notfound)
fi

# look for "struct ufs_specific" (ultrix)
if test "$ac_cv_type_ufs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct ufs_specific a;
], ac_cv_type_ufs_args="struct ufs_specific", ac_cv_type_ufs_args=notfound)
fi

])
if test "$ac_cv_type_ufs_args" != notfound
then
  AC_DEFINE_UNQUOTED(ufs_args_t, $ac_cv_type_ufs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check for type of xdrproc_t (usually in <rpc/xdr.h>)
AC_DEFUN([AMU_TYPE_XDRPROC_T],
[
AC_CACHE_CHECK(for xdrproc_t,
ac_cv_type_xdrproc_t,
[
# try to compile a program which may have a definition for the type
dnl need a series of compilations, which will test out every possible type
# look for "xdrproc_t"
AC_TRY_COMPILE_RPC(
[ xdrproc_t xdr_int;
], ac_cv_type_xdrproc_t=yes, ac_cv_type_xdrproc_t=no)
])
if test "$ac_cv_type_xdrproc_t" = yes
then
  AC_DEFINE_UNQUOTED(XDRPROC_T_TYPE, xdrproc_t)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl Find the correct type for XFS mount(2) arguments structure
AC_DEFUN([AMU_TYPE_XFS_ARGS],
[
AC_CACHE_CHECK(for structure type of xfs mount(2) arguments,
ac_cv_type_xfs_args,
[
# set to a default value
ac_cv_type_xfs_args=notfound

# look for "struct xfs_args"
if test "$ac_cv_type_xfs_args" = notfound
then
AC_TRY_COMPILE_ANYFS(
[ struct xfs_args a;
], ac_cv_type_xfs_args="struct xfs_args", ac_cv_type_xfs_args=notfound)
fi

])
if test "$ac_cv_type_xfs_args" != notfound
then
  AC_DEFINE_UNQUOTED(xfs_args_t, $ac_cv_type_xfs_args)
fi
])
dnl ======================================================================


dnl ######################################################################
dnl check the correct type for the 3rd argument to yp_order()
AC_DEFUN([AMU_TYPE_YP_ORDER_OUTORDER],
[
AC_CACHE_CHECK(pointer type of 3rd argument to yp_order(),
ac_cv_yp_order_outorder,
[
# select the correct type
case "${host_os}" in
	aix[[1-3]]* | aix4.[[0-2]]* | sunos[[34]]* | solaris1* )
		ac_cv_yp_order_outorder=int ;;
	solaris* | svr4* | sysv4* | sunos* | hpux* | aix* )
		ac_cv_yp_order_outorder="unsigned long" ;;
	osf* )
		# DU4 man page is wrong, headers are right
		ac_cv_yp_order_outorder="unsigned int" ;;
	* )
		ac_cv_yp_order_outorder=int ;;
esac
])
AC_DEFINE_UNQUOTED(YP_ORDER_OUTORDER_TYPE, $ac_cv_yp_order_outorder)
])
dnl ======================================================================


dnl ######################################################################
dnl Do we want to compile with "ADDON" support? (hesiod, ldap, etc.)
AC_DEFUN([AMU_WITH_ADDON],
[AC_MSG_CHECKING([if $1 is wanted])
ac_upcase=`echo $1|tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
AC_ARG_WITH($1,
 AC_HELP_STRING([--with-$1],
		[enable $2 support (default=yes if found)]
),[
if test "$withval" = "yes"; then
  with_$1=yes
elif test "$withval" = "no"; then
  with_$1=no
else
  AC_MSG_ERROR(please use \"yes\" or \"no\" with --with-$1)
fi
],[
with_$1=yes
])
if test "$with_$1" = "yes"
then
  AC_MSG_RESULT([yes, will enable if all libraries are found])
else
  AC_MSG_RESULT([no])
fi
])


dnl ######################################################################
dnl end of aclocal.m4 for am-utils-6.x

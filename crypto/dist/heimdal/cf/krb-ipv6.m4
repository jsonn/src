dnl $Id: krb-ipv6.m4,v 1.1.1.1.2.1 2001/04/05 23:22:57 he Exp $
dnl
dnl test for IPv6
dnl
AC_DEFUN(AC_KRB_IPV6, [
AC_ARG_WITH(ipv6,
[  --without-ipv6	do not enable IPv6 support],[
if test "$withval" = "no"; then
	ac_cv_lib_ipv6=no
fi])
AC_CACHE_VAL(ac_cv_lib_ipv6,
[dnl check for different v6 implementations (by itojun)
v6type=unknown
v6lib=none

AC_MSG_CHECKING([ipv6 stack type])
for i in v6d toshiba kame inria zeta linux; do
	case $i in
	v6d)
		AC_EGREP_CPP(yes, [
#include </usr/local/v6/include/sys/types.h>
#ifdef __V6D__
yes
#endif],
			[v6type=$i; v6lib=v6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-I/usr/local/v6/include $CFLAGS"])
		;;
	toshiba)
		AC_EGREP_CPP(yes, [
#include <sys/param.h>
#ifdef _TOSHIBA_INET6
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	kame)
		AC_EGREP_CPP(yes, [
#include <netinet/in.h>
#ifdef __KAME__
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	inria)
		AC_EGREP_CPP(yes, [
#include <netinet/in.h>
#ifdef IPV6_INRIA_VERSION
yes
#endif],
			[v6type=$i; CFLAGS="-DINET6 $CFLAGS"])
		;;
	zeta)
		AC_EGREP_CPP(yes, [
#include <sys/param.h>
#ifdef _ZETA_MINAMI_INET6
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	linux)
		if test -d /usr/inet6; then
			v6type=$i
			v6lib=inet6
			v6libdir=/usr/inet6
			CFLAGS="-DINET6 $CFLAGS"
		fi
		;;
	esac
	if test "$v6type" != "unknown"; then
		break
	fi
done
AC_MSG_RESULT($v6type)

if test "$v6lib" != "none"; then
	for dir in $v6libdir /usr/local/v6/lib /usr/local/lib; do
		if test -d $dir -a -f $dir/lib$v6lib.a; then
			LIBS="-L$dir -l$v6lib $LIBS"
			break
		fi
	done
fi
AC_TRY_LINK([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
],
[
 struct sockaddr_in6 sin6;
 int s;

 s = socket(AF_INET6, SOCK_DGRAM, 0);

 sin6.sin6_family = AF_INET6;
 sin6.sin6_port = htons(17);
 sin6.sin6_addr = in6addr_any;
 bind(s, (struct sockaddr *)&sin6, sizeof(sin6));
],
ac_cv_lib_ipv6=yes,
ac_cv_lib_ipv6=no)])
AC_MSG_CHECKING(for IPv6)
AC_MSG_RESULT($ac_cv_lib_ipv6)
if test "$ac_cv_lib_ipv6" = yes; then
  AC_DEFINE(HAVE_IPV6, 1, [Define if you have IPv6.])
fi
])

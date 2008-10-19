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
#ifdef UTS_RELEASE
  printf("%s", UTS_RELEASE);
#else
# define AMU_MA(a) ((a) >> 16)
# define AMU_MI(a) (((a) & 0xffff) >> 8)
# define AMU_PL(a) ((a) & 0xff)
 printf("%d.%d.%d", AMU_MA(LINUX_VERSION_CODE), AMU_MI(LINUX_VERSION_CODE), AMU_PL(LINUX_VERSION_CODE));
#endif
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

/*	$NetBSD: linux32_missing.c,v 1.5.12.1 2008/05/10 23:48:56 wrstuden Exp $ */

#include <sys/cdefs.h>

#include "opt_compat_linux32.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/lwp.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>

#include <compat/netbsd32/netbsd32.h>

#include <compat/linux32/common/linux32_types.h>
#include <compat/linux32/common/linux32_signal.h>

#include <compat/linux32/arch/amd64/linux32_missing.h>
#include <compat/linux32/arch/amd64/linux32_syscallargs.h>

#include <compat/linux/common/linux_fcntl64.c>
#include <compat/linux/common/linux_llseek.c>
#include <compat/linux/common/linux_misc_notalpha.c>
#include <compat/linux/common/linux_misc.c>
#include <compat/linux/common/linux_uid16.c>

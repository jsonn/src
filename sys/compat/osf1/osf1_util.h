/*	$NetBSD: osf1_util.h,v 1.3.10.1 1999/06/21 01:08:41 thorpej Exp $	*/

#ifndef _COMPAT_OSF1_OSF1_UTIL_H_
#define _COMPAT_OSF1_OSF1_UTIL_H_

#include <compat/common/compat_util.h>

extern const char osf1_emul_path[];

#define OSF1_CHECK_ALT_EXIST(p, sgp, path) \
    CHECK_ALT_EXIST(p, sgp, osf1_emul_path, path)

#define OSF1_CHECK_ALT_CREAT(p, sgp, path) \
    CHECK_ALT_CREAT(p, sgp, osf1_emul_path, path)

#endif /* _COMPAT_OSF1_OSF1_UTIL_H_ */

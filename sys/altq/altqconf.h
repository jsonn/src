/*	$NetBSD: altqconf.h,v 1.1.4.2 2002/09/17 21:12:20 nathanw Exp $	*/

#if defined(_KERNEL_OPT)
#include "opt_altq_enabled.h"

#include <sys/conf.h>

#ifdef ALTQ
#define	NALTQ	1
#else
#define	NALTQ	0
#endif

#endif /* _KERNEL_OPT */

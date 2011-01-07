/*	$NetBSD: vmparam.h,v 1.3.134.1 2011/01/07 01:40:36 matt Exp $	*/

#ifdef _KERNEL_OPT
#include "opt_ppcarch.h"
#endif

#ifdef PPC_IBM4XX
#include <powerpc/ibm4xx/vmparam.h>
#elif defined(PPC_OEA)
#include <powerpc/oea/vmparam.h>
#elif defined(PPC_BOOKE)
#include <powerpc/booke/vmparam.h>
#endif

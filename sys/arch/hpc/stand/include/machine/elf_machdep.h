/*	$NetBSD: elf_machdep.h,v 1.1.26.1 2004/08/12 11:41:11 skrll Exp $	*/

/* Windows CE architecture */
#define	ELFSIZE		32

#ifdef MIPS
#include "../../../../hpcmips/include/elf_machdep.h"
#endif
#ifdef SHx
#include "../../../../hpcsh/include/elf_machdep.h"
#endif
#ifdef ARM
#include "../../../../arm/include/elf_machdep.h"
#endif

/*      $NetBSD: bswap.h,v 1.2.2.1 2000/11/20 20:32:47 bouyer Exp $      */

/* Written by Manuel Bouyer. Public domain */

#ifndef _MACHINE_BSWAP_H_
#define	_MACHINE_BSWAP_H_

#define __BSWAP_RENAME
#include <sys/bswap.h>

#ifdef  __GNUC__

#include <machine/byte_swap.h>
#define bswap16(x)      __byte_swap_word(x)
#define bswap32(x)      __byte_swap_long(x)

#endif /* __GNUC__ */

#endif /* !_MACHINE_BSWAP_H_ */

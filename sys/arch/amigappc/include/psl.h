/*	$NetBSD: psl.h,v 1.1.134.2 2010/03/11 15:02:02 yamt Exp $ */

#ifndef	_AMIGAPPC_PSL_H_
#define	_AMIGAPPC_PSL_H_

#include <powerpc/psl.h>

/*
 * Compatibility with m68k/include/psl.h for amiga/68k devices.
 * Has to match with interrupt IPLs in amigappc_install_handlers().
 */
#define spl1()		splbio()
#define spl2()		splbio()
#define spl3()		spltty()
#define spl4()		splaudio()
#define spl5()		splserial()
#define spl6()		splserial()
#define spl7()		splhigh()

#endif /* _AMIGAPPC_PSL_H_ */

/*	$NetBSD: types.h,v 1.1.2.2 2001/02/11 19:10:21 bouyer Exp $	*/

/* Windows CE architecture */

#ifndef	_MACHTYPES_H_
#define	_MACHTYPES_H_

#include <sys/cdefs.h>
#include <machine/int_types.h>

typedef unsigned char		u_int8_t;
typedef unsigned short		u_int16_t;
typedef unsigned int		u_int32_t;
typedef unsigned __int64	u_int64_t;

typedef signed char		int8_t;	
typedef signed short		int16_t;
typedef signed int		int32_t;
typedef signed __int64		int64_t;

typedef u_int32_t		off_t;
#define off_t			u_int32_t
#define _TIME_T_DEFINED
typedef unsigned long		time_t;
typedef unsigned int		size_t;

/* Windows CE virtual address */
typedef u_int32_t		vaddr_t;
typedef u_int32_t		vsize_t;
/* Physical address */
typedef u_int32_t		paddr_t;
typedef u_int32_t		psize_t;

/* kernel virtual address */
typedef u_int32_t		kaddr_t;
typedef u_int32_t		ksize_t;

#endif /* _MACHTYPES_H_ */

/* $NetBSD: tga_conf.c,v 1.4.4.1 1997/09/04 00:54:04 thorpej Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: tga_conf.c,v 1.4.4.1 1997/09/04 00:54:04 thorpej Exp $");

#include <sys/param.h>
#include <sys/device.h>

#include <dev/pci/pcivar.h>
#include <machine/tgareg.h>
#include <alpha/pci/tgavar.h>

#undef KB
#define KB		* 1024
#undef MB
#define	MB		* 1024 * 1024

static const struct tga_conf tga_configs[TGA_TYPE_UNKNOWN] = {
	/* TGA_TYPE_T8_01 */
	{
		"T8-01",
		&tga_ramdac_bt485,
		8,
		4 MB,
		2 KB,
		1,	{  2 MB,     0 },	{ 1 MB,    0 },
		0,	{     0,     0 },	{    0,    0 },
	},
	/* TGA_TYPE_T8_02 */
	{
		"T8-02",
		&tga_ramdac_bt485,
		8,
		4 MB,
		4 KB,
		1,	{  2 MB,     0 },	{ 2 MB,    0 },
		0,	{     0,     0 },	{    0,    0 },
	},
	/* TGA_TYPE_T8_22 */
	{
		"T8-22",
		&tga_ramdac_bt485,
		8,
		8 MB,
		4 KB,
		1,	{  4 MB,     0 },	{ 2 MB,    0 },
		1,	{  6 MB,     0 },	{ 2 MB,    0 },
	},
	/* TGA_TYPE_T8_44 */
	{
		"T8-44",
		&tga_ramdac_bt485,
		8,
		16 MB,
		4 KB,
		2,	{  8 MB, 12 MB },	{ 2 MB, 2 MB },
		2,	{ 10 MB, 14 MB },	{ 2 MB, 2 MB },
	},
	/* TGA_TYPE_T32_04 */
	{
		"T32-04",
		&tga_ramdac_bt463,
		32,
		16 MB,
		8 KB,
		1,	{  8 MB,     0 },	{ 4 MB,    0 },
		0,	{     0,     0 },	{    0,    0 },
	},
	/* TGA_TYPE_T32_08 */
	{
		"T32-08",
		&tga_ramdac_bt463,
		32,
		16 MB,
		16 KB,
		1,	{  8 MB,    0 },	{ 8 MB,    0 },
		0,	{     0,    0 },	{    0,    0 },
	},
	/* TGA_TYPE_T32_88 */
	{
		"T32-88",
		&tga_ramdac_bt463,
		32,
		32 MB,
		16 KB,
		1,	{ 16 MB,    0 },	{ 8 MB,    0 },
		1,	{ 24 MB,    0 },	{ 8 MB,    0 },
	},
};

#undef KB
#undef MB

int
tga_identify(regs)
	tga_reg_t *regs;
{
	int type;
	int deep, addrmask, wide;

	deep = (regs[TGA_REG_GDER] & 0x1) != 0;		/* XXX */
	addrmask = ((regs[TGA_REG_GDER] >> 2) & 0x7);	/* XXX */
	wide = (regs[TGA_REG_GDER] & 0x200) == 0;	/* XXX */

	type = TGA_TYPE_UNKNOWN;

	if (!deep) {
		/* 8bpp frame buffer */

		if (addrmask == 0x0) {
			/* 4MB core map; T8-01 or T8-02 */

			if (!wide)
				type = TGA_TYPE_T8_01;
			else
				type = TGA_TYPE_T8_02;
		} else if (addrmask == 0x1) {
			/* 8MB core map; T8-22 */

			if (wide)			/* sanity */
				type = TGA_TYPE_T8_22;
		} else if (addrmask == 0x3) {
			/* 16MB core map; T8-44 */

			if (wide)			/* sanity */
				type = TGA_TYPE_T8_44;
		}
	} else {
		/* 32bpp frame buffer */

		if (addrmask == 0x3) {
			/* 16MB core map; T32-04 or T32-08 */

			if (!wide)
				type = TGA_TYPE_T32_04;
			else
				type = TGA_TYPE_T32_08;
		} else if (addrmask == 0x7) {
			/* 32MB core map; T32-88 */

			if (wide)			/* sanity */
				type = TGA_TYPE_T32_88;
		}
	}

	return (type);
}

const struct tga_conf *
tga_getconf(type)
	int type;
{

	if (type >= 0 && type < TGA_TYPE_UNKNOWN)
		return &tga_configs[type];

	return (NULL);
}


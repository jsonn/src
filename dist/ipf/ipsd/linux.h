/*	$NetBSD: linux.h,v 1.1.1.1.4.1 1999/12/27 18:27:57 wrstuden Exp $	*/

/*
 * Copyright (C) 1997-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)linux.h	1.1 8/19/95
 */

#include <linux/config.h>
#ifdef MODULE
#include <linux/module.h>
#include <linux/version.h>
#endif /* MODULE */

#include "ip_compat.h"

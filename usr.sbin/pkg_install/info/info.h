/* $NetBSD: info.h,v 1.12.4.1 2002/06/26 16:54:13 he Exp $ */

/* from FreeBSD Id: info.h,v 1.10 1997/02/22 16:09:40 peter Exp */

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 23 August 1993
 *
 * Include and define various things wanted by the info command.
 *
 */

#ifndef _INST_INFO_H_INCLUDE
#define _INST_INFO_H_INCLUDE

#ifndef MAXINDEXSIZE
#define MAXINDEXSIZE 60
#endif

#ifndef MAXNAMESIZE
#define MAXNAMESIZE  20
#endif

#define SHOW_COMMENT		0x00001
#define SHOW_DESC		0x00002
#define SHOW_PLIST		0x00004
#define SHOW_INSTALL		0x00008
#define SHOW_DEINSTALL		0x00010
#define SHOW_REQUIRE		0x00020
#define SHOW_PREFIX		0x00040
#define SHOW_INDEX		0x00080
#define SHOW_FILES		0x00100
#define SHOW_DISPLAY		0x00200
#define SHOW_REQBY		0x00400
#define SHOW_MTREE		0x00800
#define SHOW_BUILD_VERSION	0x01000
#define SHOW_BUILD_INFO		0x02000
#define SHOW_DEPENDS		0x04000
#define SHOW_PKG_SIZE		0x08000
#define SHOW_ALL_SIZE		0x10000

extern int Flags;
extern Boolean AllInstalled;
extern Boolean File2Pkg;
extern Boolean Quiet;
extern char *InfoPrefix;
extern char PlayPen[];
extern size_t PlayPenSize;
extern char *CheckPkg;
extern size_t termwidth;
extern lpkg_head_t pkgs;

extern void show_file(char *, char *);
extern void show_plist(char *, package_t *, pl_ent_t);
extern void show_files(char *, package_t *);
extern void show_depends(char *, package_t *);
extern void show_index(char *, char *);

#endif				/* _INST_INFO_H_INCLUDE */

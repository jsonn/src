/* $NetBSD: delete.h,v 1.6.8.1 2002/06/26 16:53:27 he Exp $ */

/* from FreeBSD Id: delete.h,v 1.4 1997/02/22 16:09:35 peter Exp */

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
 * 18 July 1993
 *
 * Include and define various things wanted by the delete command.
 *
 */

#ifndef _INST_DELETE_H_INCLUDE
#define _INST_DELETE_H_INCLUDE

extern char *Prefix;
extern char *ProgramPath;
extern Boolean NoDeInstall;
extern Boolean CleanDirs;
extern Boolean Force;
extern Boolean Recurse_up;
extern Boolean Recurse_down;
extern lpkg_head_t pkgs;

#endif				/* _INST_DELETE_H_INCLUDE */

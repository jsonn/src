/* $NetBSD: add.h,v 1.3.2.2 2000/07/31 18:18:17 he Exp $ */

/* from FreeBSD Id: add.h,v 1.8 1997/02/22 16:09:15 peter Exp  */

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
 * Include and define various things wanted by the add command.
 *
 */

#ifndef _INST_ADD_H_INCLUDE
#define _INST_ADD_H_INCLUDE

typedef enum {
	NORMAL, MASTER, SLAVE
}       add_mode_t;

extern char *Prefix;
extern Boolean NoInstall;
extern Boolean NoRecord;
extern Boolean Force;
extern char *Mode;
extern char *Owner;
extern char *Group;
extern char *Directory;
extern char *PkgName;
extern char FirstPen[];
extern add_mode_t AddMode;

int     make_hierarchy(char *);
int     extract_plist(char *, package_t *);
void    apply_perms(char *, char *);

#endif				/* _INST_ADD_H_INCLUDE */

/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: xdbm.h,v 1.1.1.1.2.1 2001/04/05 23:23:16 he Exp $ */

/* Generic *dbm include file */

#ifndef __XDBM_H__
#define __XDBM_H__

#if defined(HAVE_DB_H)
#define DB_DBM_HSEARCH 1
#include <db.h>
#endif

#ifndef DBM_INSERT
#if defined(HAVE_NDBM_H)
#include <ndbm.h>
#elif defined(HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined(HAVE_DBM_H)
#include <dbm.h>
#elif defined(HAVE_RPCSVC_DBM_H)
#include <rpcsvc/dbm.h>
#endif
#endif

/* Macros to convert ndbm names to dbm names.
 * Note that dbm_nextkey() cannot be simply converted using a macro, since
 * it is invoked giving the database, and nextkey() needs the previous key.
 *
 * Instead, all routines call "dbm_next" instead.
 */

#if !defined(NDBM) && !defined(HAVE_DB_H)
typedef char DBM;

#define dbm_open(file, flags, mode) ((dbminit(file) == 0)?"":((char *)0))
#define dbm_fetch(db, key) fetch(key)
#define dbm_store(db, key, content, flag) store(key, content)
#define dbm_delete(db, key) delete(key)
#define dbm_firstkey(db) firstkey()
#define dbm_next(db,key) nextkey(key)
#define dbm_close(db) dbmclose()
#else
#define dbm_next(db,key) dbm_nextkey(db)
#endif

#endif /* __XDBM_H__ */

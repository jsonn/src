/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska H�gskolan
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

#include "krb_locl.h"

RCSID("$Id: get_default_principal.c,v 1.1.1.1.4.2 2000/06/16 18:45:53 thorpej Exp $");

int
krb_get_default_principal(char *name, char *instance, char *realm)
{
  char *file;
  int ret;
  char *p;

  if ((file = getenv("KRBTKFILE")) == NULL)
      file = TKT_FILE;  
  
  ret = krb_get_tf_fullname(file, name, instance, realm);
  if(ret == KSUCCESS)
      return 0;

  p = getenv("KRB4PRINCIPAL");
  if(p && kname_parse(name, instance, realm, p) == KSUCCESS)
      return 1;

#ifdef HAVE_PWD_H
  {
    struct passwd *pw;
    pw = getpwuid(getuid());
    if(pw == NULL){
      return -1;
    }

    strlcpy (name, pw->pw_name, ANAME_SZ);
    strlcpy (instance, "", INST_SZ);
    krb_get_lrealm(realm, 1);

    if(strcmp(name, "root") == 0) {
      p = NULL;
#if defined(HAVE_GETLOGIN) && !defined(POSIX_GETLOGIN)
      p = getlogin();
#endif
      if(p == NULL)
	p = getenv("USER");
      if(p == NULL)
	p = getenv("LOGNAME");
      if(p){
	  strlcpy (name, p, ANAME_SZ);
	  strlcpy (instance, "root", INST_SZ);
      }
    }
    return 1;
  }
#else
  return -1;
#endif
}

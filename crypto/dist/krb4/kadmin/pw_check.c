/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska H�gskolan
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

#include "kadm_locl.h"

RCSID("$Id: pw_check.c,v 1.1.1.1.4.2 2000/06/16 18:46:07 thorpej Exp $");

/*
 * kadm_pw_check
 *
 * pw		: new password or "" if none passed
 * newkey	: key for pw as passed from client
 * strings	: interesting strings to check for
 *
 * returns NULL if pw is ok, else an explanatory string
 */
int
kadm_pw_check(char *pw, des_cblock *newkey, char **pw_msg, 
	      char **strings)
{
  des_cblock pwkey;
  int status=KADM_SUCCESS;
  
  if (pw == NULL || *pw == '\0')
    return status;		/* XXX - Change this later */

#ifndef NO_PW_CHECK
  *pw_msg = NULL;
  des_string_to_key(pw, &pwkey); /* Check AFS string to key also! */
  if (memcmp(pwkey, *newkey, sizeof(pwkey)) != 0)
    {
      /* no password or bad key */
      status=KADM_PW_MISMATCH;
      *pw_msg = "Password doesn't match supplied DES key";
    }
  else if (strlen(pw) < MIN_KPW_LEN)
    {
      status = KADM_INSECURE_PW;
      *pw_msg="Password is too short";
    }
  
#ifdef DICTPATH
  *pw_msg = FascistCheck(pw, DICTPATH, strings);
  if (*pw_msg)
    return KADM_INSECURE_PW;
#endif

  memset(pwkey, 0, sizeof(pwkey));
#endif

  return status;
}

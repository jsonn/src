/*	$NetBSD: ieee80211_compat.h,v 1.5.4.2 2004/08/03 10:54:21 skrll Exp $	*/
/*-
 * Copyright (c) 2003, 2004 David Young
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _NET80211_IEEE80211_COMPAT_H_
#define _NET80211_IEEE80211_COMPAT_H_

#ifdef __NetBSD__
#define IASSERT(cond, complaint) if (!(cond)) panic complaint
#else
#define IASSERT(cond, complaint) KASSERT(cond, complaint)
#endif

#ifdef __NetBSD__
void if_printf(struct ifnet *, const char *, ...)
    __attribute__((__format__(__printf__,2,3)));
#endif

#ifdef __NetBSD__
#define ieee80211_node_critsec_decl(v) int v
#define ieee80211_node_critsec_begin(ic, v) do { v = splnet(); } while (0)
#define ieee80211_node_critsec_end(ic, v) splx(v)
#else
#define ieee80211_node_critsec_decl(v) /* empty */
#define ieee80211_node_critsec_begin(ic, v) mtx_lock(&(ic)->ic_nodelock)
#define ieee80211_node_critsec_end(ic, v) mtx_unlock(&ic->ic_nodelock)
#endif

#endif /* _NET80211_IEEE80211_COMPAT_H_ */

/*	$NetBSD: prop_array.h,v 1.4.4.1 2007/09/27 16:16:29 xtraeme Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PROPLIB_PROP_ARRAY_H_
#define	_PROPLIB_PROP_ARRAY_H_

#include <prop/prop_object.h>

typedef struct _prop_array *prop_array_t;

__BEGIN_DECLS
prop_array_t	prop_array_create(void);
prop_array_t	prop_array_create_with_capacity(unsigned int);

prop_array_t	prop_array_copy(prop_array_t);
prop_array_t	prop_array_copy_mutable(prop_array_t);

unsigned int	prop_array_capacity(prop_array_t);
unsigned int	prop_array_count(prop_array_t);
bool		prop_array_ensure_capacity(prop_array_t, unsigned int);

void		prop_array_make_immutable(prop_array_t);
bool		prop_array_mutable(prop_array_t);

prop_object_iterator_t prop_array_iterator(prop_array_t);

prop_object_t	prop_array_get(prop_array_t, unsigned int);
bool		prop_array_set(prop_array_t, unsigned int, prop_object_t);
bool		prop_array_add(prop_array_t, prop_object_t);
void		prop_array_remove(prop_array_t, unsigned int);

bool		prop_array_equals(prop_array_t, prop_array_t);

char *		prop_array_externalize(prop_array_t);
prop_array_t	prop_array_internalize(const char *);

bool		prop_array_externalize_to_file(prop_array_t, const char *);
prop_array_t	prop_array_internalize_from_file(const char *);

#if defined(__NetBSD__)
#if !defined(_KERNEL) && !defined(_STANDALONE)
int		prop_array_send_ioctl(prop_array_t, int, unsigned long);
int		prop_array_recv_ioctl(int, unsigned long, prop_array_t *);
#elif defined(_KERNEL)
struct plistref;

int		prop_array_copyin_ioctl(const struct plistref *, const u_long,
					prop_array_t *);
int		prop_array_copyout_ioctl(struct plistref *, const u_long,
					 prop_array_t);
#endif
#endif /* __NetBSD__ */

__END_DECLS

#endif /* _PROPLIB_PROP_ARRAY_H_ */

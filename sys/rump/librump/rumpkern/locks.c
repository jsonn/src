/*	$NetBSD: locks.c,v 1.1.6.2 2007/11/06 23:34:36 matt Exp $	*/

/*
 * Copyright (c) 2007 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>

#include "rumpuser.h"

void
mutex_init(kmutex_t *mtx, kmutex_type_t type, int ipl)
{

	rumpuser_mutex_init(&mtx->kmtx_mtx);
}

void
mutex_destroy(kmutex_t *mtx)
{

	rumpuser_mutex_destroy(mtx->kmtx_mtx);
}

void
mutex_enter(kmutex_t *mtx)
{

	rumpuser_mutex_enter(mtx->kmtx_mtx);
}

int
mutex_tryenter(kmutex_t *mtx)
{
	int rv;

	rv = rumpuser_mutex_tryenter(mtx->kmtx_mtx);
	if (rv)
		return 0;
	else
		return 1;
}

void
mutex_exit(kmutex_t *mtx)
{

	rumpuser_mutex_exit(mtx->kmtx_mtx);
}

int
mutex_owned(kmutex_t *mtx)
{

	/* XXX */
	return 1;
}

/* reader/writer locks */

void
rw_init(krwlock_t *rw)
{

	rumpuser_rw_init(&rw->krw_pthlock);
}

void
rw_destroy(krwlock_t *rw)
{

	rumpuser_rw_destroy(rw->krw_pthlock);
}

void
rw_enter(krwlock_t *rw, const krw_t op)
{

	rumpuser_rw_enter(rw->krw_pthlock, op == RW_WRITER);
}

int
rw_tryenter(krwlock_t *rw, const krw_t op)
{

	return rumpuser_rw_tryenter(rw->krw_pthlock, op == RW_WRITER);
}

void
rw_exit(krwlock_t *rw)
{

	rumpuser_rw_exit(rw->krw_pthlock);
}

/* always fails */
int
rw_tryupgrade(krwlock_t *rw)
{

	return 0;
}

/* curriculum vitaes */

/* forgive me for I have sinned */
#define RUMPCV(a) ((struct rumpuser_cv *)(__UNCONST((a)->cv_wmesg)))

void
cv_init(kcondvar_t *cv, const char *msg)
{

	rumpuser_cv_init((struct rumpuser_cv **)__UNCONST(&cv->cv_wmesg));
}

void
cv_destroy(kcondvar_t *cv)
{

	rumpuser_cv_destroy(RUMPCV(cv));
}

void
cv_wait(kcondvar_t *cv, kmutex_t *mtx)
{

	rumpuser_cv_wait(RUMPCV(cv), mtx->kmtx_mtx);
}

void
cv_signal(kcondvar_t *cv)
{

	rumpuser_cv_signal(RUMPCV(cv));
}

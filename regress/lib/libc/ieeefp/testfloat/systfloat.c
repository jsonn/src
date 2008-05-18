/*	$NetBSD: systfloat.c,v 1.7.24.1 2008/05/18 12:30:45 yamt Exp $	*/

/* This is a derivative work. */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ross Harvey.
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

/*
===============================================================================

This C source file is part of TestFloat, Release 2a, a package of programs
for testing the correctness of floating-point arithmetic complying to the
IEC/IEEE Standard for Floating-Point.

Written by John R. Hauser.  More information is available through the Web
page `http://HTTP.CS.Berkeley.EDU/~jhauser/arithmetic/TestFloat.html'.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort
has been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT
TIMES RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO
PERSONS AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ANY
AND ALL LOSSES, COSTS, OR OTHER PROBLEMS ARISING FROM ITS USE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) they include prominent notice that the work is derivative, and (2) they
include prominent notice akin to these four paragraphs for those parts of
this code that are retained.

===============================================================================
*/

#include <sys/cdefs.h>
#ifndef __lint
__RCSID("$NetBSD: systfloat.c,v 1.7.24.1 2008/05/18 12:30:45 yamt Exp $");
#endif

#include <math.h>
#include <ieeefp.h>
#include "milieu.h"
#include "softfloat.h"
#include "systfloat.h"
#include "systflags.h"
#include "systmodes.h"

typedef union {
    float32 f32;
    float f;
} union32;
typedef union {
    float64 f64;
    double d;
} union64;
#if defined( FLOATX80 ) && defined( LONG_DOUBLE_IS_FLOATX80 )
typedef union {
    floatx80 fx80;
    long double ld;
} unionx80;
#endif
#if defined( FLOAT128 ) && defined( LONG_DOUBLE_IS_FLOAT128 )
typedef union {
    float128 f128;
    long double ld;
} union128;
#endif

fp_except
syst_float_flags_clear(void)
{
    return fpsetsticky(0)
	& (FP_X_IMP | FP_X_UFL | FP_X_OFL | FP_X_DZ | FP_X_INV);
}

void
syst_float_set_rounding_mode(fp_rnd direction)
{
    fpsetround(direction);
    fpsetmask(0);
}

float32 syst_int32_to_float32( int32 a )
{
    const union32 uz = { .f = a };

    return uz.f32;

}

float64 syst_int32_to_float64( int32 a )
{
    const union64 uz = { .d = a };

    return uz.f64;

}

#if defined( FLOATX80 ) && defined( LONG_DOUBLE_IS_FLOATX80 )

floatx80 syst_int32_to_floatx80( int32 a )
{
    const unionx80 uz = { .ld = a };

    return uz.fx80;

}

#endif

#if defined( FLOAT128 ) && defined( LONG_DOUBLE_IS_FLOAT128 )

float128 syst_int32_to_float128( int32 a )
{
    const union128 uz = { .ld = a };

    return uz.f128;

}

#endif

#ifdef BITS64

float32 syst_int64_to_float32( int64 a )
{
    const union32 uz = { .f = a };

    return uz.f32;
}

float64 syst_int64_to_float64( int64 a )
{
    const union64 uz = { .d = a };

    return uz.f64;
}

#if defined( FLOATX80 ) && defined( LONG_DOUBLE_IS_FLOATX80 )

floatx80 syst_int64_to_floatx80( int64 a )
{
    const unionx80 uz = { .ld = a };

    return uz.fx80;
}

#endif

#if defined( FLOAT128 ) && defined( LONG_DOUBLE_IS_FLOAT128 )

float128 syst_int64_to_float128( int64 a )
{
    const union128 uz = { .ld = a };

    return uz.f128;
}

#endif

#endif

int32 syst_float32_to_int32_round_to_zero( float32 a )
{
    const union32 uz = { .f32 = a };

    return uz.f;

}

#ifdef BITS64

int64 syst_float32_to_int64_round_to_zero( float32 a )
{
    const union32 uz = { .f32 = a };

    return uz.f;
}

#endif

float64 syst_float32_to_float64( float32 a )
{
    const union32 ua = { .f32 = a };
    union64 uz;

    uz.d = ua.f;
    return uz.f64;

}

#if defined( FLOATX80 ) && defined( LONG_DOUBLE_IS_FLOATX80 )

floatx80 syst_float32_to_floatx80( float32 a )
{
    const union32 ua = { .f32 = a };
    unionx80 uz;

    uz.ld = ua.f;
    return uz.fx80;
}

#endif

#if defined( FLOAT128 ) && defined( LONG_DOUBLE_IS_FLOAT128 )

float128 syst_float32_to_float128( float32 a )
{
    const union32 ua = { .f32 = a };
    union128 ub;

    ub.ld = ua.f;
    return ub.f128;
}

#endif

float32 syst_float32_add( float32 a, float32 b )
{
    const union32 ua = { .f32 = a }, ub = { .f32 = b };
    union32 uz;

    uz.f = ua.f + ub.f;
    return uz.f32;
}

float32 syst_float32_sub( float32 a, float32 b )
{
    const union32 ua = { .f32 = a }, ub = { .f32 = b };
    union32 uz;

    uz.f = ua.f - ub.f;
    return uz.f32;
}

float32 syst_float32_mul( float32 a, float32 b )
{
    const union32 ua = { .f32 = a }, ub = { .f32 = b };
    union32 uz;

    uz.f = ua.f * ub.f;
    return uz.f32;
}

float32 syst_float32_div( float32 a, float32 b )
{
    const union32 ua = { .f32 = a }, ub = { .f32 = b };
    union32 uz;

    uz.f = ua.f / ub.f;
    return uz.f32;
}

flag syst_float32_eq( float32 a, float32 b )
{
    const union32 ua = { .f32 = a }, ub = { .f32 = b };

    return ua.f == ub.f;
}

flag syst_float32_le( float32 a, float32 b )
{
    const union32 ua = { .f32 = a }, ub = { .f32 = b };

    return ua.f <= ub.f;
}

flag syst_float32_lt( float32 a, float32 b )
{
    const union32 ua = { .f32 = a }, ub = { .f32 = b };

    return ua.f < ub.f;
}

int32 syst_float64_to_int32_round_to_zero( float64 a )
{
    const union64 uz = { .f64 = a };

    return uz.d;
}

#ifdef BITS64

int64 syst_float64_to_int64_round_to_zero( float64 a )
{
    const union64 uz = { .f64 = a };

    return uz.d;
}

#endif

float32 syst_float64_to_float32( float64 a )
{
    const union64 ua = { .f64 = a };
    union32 uz;

    uz.f = ua.d;
    return uz.f32;
}

#if defined( FLOATX80 ) && defined( LONG_DOUBLE_IS_FLOATX80 )

floatx80 syst_float64_to_floatx80( float64 a )
{
    const union64 ua = { .f64 = a };
    unionx80 u;

    u.ld = ua.d;
    return u.fx80;
}

#endif

#if defined( FLOAT128 ) && defined( LONG_DOUBLE_IS_FLOAT128 )

float128 syst_float64_to_float128( float64 a )
{
    const union64 ua = { .f64 = a };
    union128 uz;

    uz.ld = ua.d;
    return uz.f128;
}

#endif

float64 syst_float64_add( float64 a, float64 b )
{
    const union64 ua = { .f64 = a }, ub = { .f64 = b };
    union64 uz;

    uz.d = ua.d + ub.d;
    return uz.f64;
}

float64 syst_float64_sub( float64 a, float64 b )
{
    const union64 ua = { .f64 = a }, ub = { .f64 = b };
    union64 uz;

    uz.d = ua.d - ub.d;
    return uz.f64;
}

float64 syst_float64_mul( float64 a, float64 b )
{
    const union64 ua = { .f64 = a }, ub = { .f64 = b };
    union64 uz;

    uz.d = ua.d * ub.d;
    return uz.f64;
}

float64 syst_float64_div( float64 a, float64 b )
{
    const union64 ua = { .f64 = a }, ub = { .f64 = b };
    union64 uz;

    uz.d = ua.d / ub.d;
    return uz.f64;
}

float64 syst_float64_sqrt( float64 a )
{
    const union64 ua = { .f64 = a };
    union64 uz;

    uz.d = sqrt(ua.d);
    return uz.f64;
}

flag syst_float64_eq( float64 a, float64 b )
{
    const union64 ua = { .f64 = a }, ub = { .f64 = b };

    return ua.d == ub.d;
}

flag syst_float64_le( float64 a, float64 b )
{
    const union64 ua = { .f64 = a }, ub = { .f64 = b };

    return ua.d <= ub.d;
}

flag syst_float64_lt( float64 a, float64 b )
{
    const union64 ua = { .f64 = a }, ub = { .f64 = b };

    return ua.d < ub.d;
}

#if defined( FLOATX80 ) && defined( LONG_DOUBLE_IS_FLOATX80 )

int32 syst_floatx80_to_int32_round_to_zero( floatx80 a )
{
    const unionx80 uz = { .fx80 = a };

    return uz.ld;
}

#ifdef BITS64

int64 syst_floatx80_to_int64_round_to_zero( floatx80 a )
{
    const unionx80 uz = { .fx80 = a };

    return uz.ld;
}

#endif

float32 syst_floatx80_to_float32( floatx80 a )
{
    const unionx80 ua = { .fx80 = a };
    union32 uz;

    uz.f = ua.ld;
    return uz.f32;
}

float64 syst_floatx80_to_float64( floatx80 a )
{
    const unionx80 ua = { .fx80 = a };
    union64 uz;

    uz.d = ua.ld;
    return uz.f64;
}

floatx80 syst_floatx80_add( floatx80 a, floatx80 b )
{
    const unionx80 ua = { .fx80 = a }, ub = { .fx80 = b };
    unionx80 uz;

    uz.ld = ua.ld + ub.ld;
    return uz.fx80;
}

floatx80 syst_floatx80_sub( floatx80 a, floatx80 b )
{
    const unionx80 ua = { .fx80 = a }, ub = { .fx80 = b };
    unionx80 uz;

    uz.ld = ua.ld - ub.ld;
    return uz.fx80;
}

floatx80 syst_floatx80_mul( floatx80 a, floatx80 b )
{
    const unionx80 ua = { .fx80 = a }, ub = { .fx80 = b };
    unionx80 uz;

    uz.ld = ua.ld * ub.ld;
    return uz.fx80;
}

floatx80 syst_floatx80_div( floatx80 a, floatx80 b )
{
    const unionx80 ua = { .fx80 = a }, ub = { .fx80 = b };
    unionx80 uz;

    uz.ld = ua.ld / ub.ld;
    return uz.fx80;
}

flag syst_floatx80_eq( floatx80 a, floatx80 b )
{
    const unionx80 ua = { .fx80 = a }, ub = { .fx80 = b };

    return ua.ld == ub.ld;
}

flag syst_floatx80_le( floatx80 a, floatx80 b )
{
    const unionx80 ua = { .fx80 = a }, ub = { .fx80 = b };

    return ua.ld <= ub.ld;
}

flag syst_floatx80_lt( floatx80 a, floatx80 b )
{
    const unionx80 ua = { .fx80 = a }, ub = { .fx80 = b };

    return ua.ld < ub.ld;
}

#endif

#if defined( FLOAT128 ) && defined( LONG_DOUBLE_IS_FLOAT128 )

int32 syst_float128_to_int32_round_to_zero( float128 a )
{
    const union128 ua = { .f128 = a };

    return ua.ld;
}

#ifdef BITS64

int64 syst_float128_to_int64_round_to_zero( float128 a )
{
    const union128 ua = { .f128 = a };

    return ua.ld;
}

#endif

float32 syst_float128_to_float32( float128 a )
{
    const union128 ua = { .f128 = a };
    union32 uz;

    uz.f = ua.ld;
    return uz.f32;

}

float64 syst_float128_to_float64( float128 a )
{
    const union128 ua = { .f128 = a };
    union64 uz;

    uz.d = ua.ld;
    return uz.f64;
}

float128 syst_float128_add( float128 a, float128 b )
{
    const union128 ua = { .f128 = a }, ub = { .f128 = b };
    union128 uz;

    uz.ld = ua.ld + ub.ld;
    return uz.f128;

}

float128 syst_float128_sub( float128 a, float128 b )
{
    const union128 ua = { .f128 = a }, ub = { .f128 = b };
    union128 uz;

    uz.ld = ua.ld - ub.ld;
    return uz.f128;
}

float128 syst_float128_mul( float128 a, float128 b )
{
    const union128 ua = { .f128 = a }, ub = { .f128 = b };
    union128 uz;

    uz.ld = ua.ld * ub.ld;
    return uz.f128;
}

float128 syst_float128_div( float128 a, float128 b )
{
    const union128 ua = { .f128 = a }, ub = { .f128 = b };
    union128 uz;

    uz.ld = ua.ld / ub.ld;
    return uz.f128;
}

flag syst_float128_eq( float128 a, float128 b )
{
    const union128 ua = { .f128 = a }, ub = { .f128 = b };

    return ua.ld == ub.ld;
}

flag syst_float128_le( float128 a, float128 b )
{
    const union128 ua = { .f128 = a }, ub = { .f128 = b };

    return ua.ld <= ub.ld;
}

flag syst_float128_lt( float128 a, float128 b )
{
    const union128 ua = { .f128 = a }, ub = { .f128 = b };

    return ua.ld < ub.ld;
}

#endif

/*	$NetBSD: msm6258var.h,v 1.4.10.2 2004/09/18 14:45:59 skrll Exp $	*/

/*
 * Copyright (c) 2001 Tetsuya Isaki. All rights reserved.
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
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * OKI MSM6258 ADPCM voice synthesizer codec.
 */

void *msm6258_codec_init (void);
int  msm6258_codec_open (void *);

void msm6258_slinear16_host_to_adpcm(void *, u_char *, int);
void msm6258_slinear16_le_to_adpcm(void *, u_char *, int);
void msm6258_slinear16_be_to_adpcm(void *, u_char *, int);
void msm6258_slinear8_to_adpcm(void *, u_char *, int);
void msm6258_ulinear8_to_adpcm(void *, u_char *, int);
void msm6258_mulaw_to_adpcm(void *, u_char *, int);

void msm6258_adpcm_to_slinear16_host(void *, u_char *, int);
void msm6258_adpcm_to_slinear16_le(void *, u_char *, int);
void msm6258_adpcm_to_slinear16_be(void *, u_char *, int);
void msm6258_adpcm_to_slinear8(void *, u_char *, int);
void msm6258_adpcm_to_ulinear8(void *, u_char *, int);
void msm6258_adpcm_to_mulaw(void *, u_char *, int);

/*	$NetBSD: pic_openpic.c,v 1.1.2.2 2007/05/03 03:18:33 macallan Exp $ */

/*-
 * Copyright (c) 2007 Michael Lorenz
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
 * 3. Neither the name of The NetBSD Foundation nor the names of its
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pic_openpic.c,v 1.1.2.2 2007/05/03 03:18:33 macallan Exp $");

#include "opt_interrupt.h"

#ifdef PIC_OPENPIC

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <uvm/uvm_extern.h>

#include <machine/pio.h>
#include <powerpc/openpic.h>

#include <dev/ofw/openfirm.h>

#include <machine/autoconf.h>
#include <arch/powerpc/pic/picvar.h>

static int  opic_irq_is_enabled(struct pic_ops *, int);
static void opic_enable_irq(struct pic_ops *, int, int);
static void opic_disable_irq(struct pic_ops *, int);
static void opic_clear_irq(struct pic_ops *, int);
static int  opic_get_irq(struct pic_ops *);
static void opic_ack_irq(struct pic_ops *, int);

struct openpic_ops {
	struct pic_ops pic;
	uint32_t enable_mask;
};

struct pic_ops *
setup_openpic(uint32_t addr, int num, int passthrough)
{
	struct openpic_ops *openpic;
	struct pic_ops *pic;
	int irq;
	u_int x;

	openpic_base = (void *)addr;
	openpic = malloc(sizeof(struct openpic_ops), M_DEVBUF, M_NOWAIT);
	KASSERT(openpic != NULL);
	pic = &openpic->pic;

	pic->pic_numintrs = num;
	pic->pic_cookie = (void *)addr;
	pic->pic_irq_is_enabled = opic_irq_is_enabled;
	pic->pic_enable_irq = opic_enable_irq;
	pic->pic_reenable_irq = opic_enable_irq;
	pic->pic_disable_irq = opic_disable_irq;
	pic->pic_clear_irq = opic_clear_irq;
	pic->pic_get_irq = opic_get_irq;
	pic->pic_ack_irq = opic_ack_irq;
	strcpy(pic->pic_name, "openpic");
	pic_add(pic);

	/* disable all interrupts */
	for (irq = 0; irq < 256; irq++)
		openpic_write(OPENPIC_SRC_VECTOR(irq), OPENPIC_IMASK);

	openpic_set_priority(0, 15);

	x = openpic_read(OPENPIC_CONFIG);
	if (passthrough) {
		x &= ~OPENPIC_CONFIG_8259_PASSTHRU_DISABLE;
	} else {
		x |= OPENPIC_CONFIG_8259_PASSTHRU_DISABLE;
	}
	openpic_write(OPENPIC_CONFIG, x);

	/* send all interrupts to CPU 0 */
	for (irq = 0; irq < ICU_LEN; irq++)
		openpic_write(OPENPIC_IDEST(irq), 1 << 0);

	for (irq = 0; irq < ICU_LEN; irq++) {
		x = irq;
		x |= OPENPIC_IMASK;
		x |= OPENPIC_POLARITY_POSITIVE;
		x |= OPENPIC_SENSE_LEVEL;
		x |= 8 << OPENPIC_PRIORITY_SHIFT;
		openpic_write(OPENPIC_SRC_VECTOR(irq), x);
	}

	openpic_set_priority(0, 0);

	/* clear all pending interrunts */
	for (irq = 0; irq < 256; irq++) {
		openpic_read_irq(0);
		openpic_eoi(0);
	}

	for (irq = 0; irq < num; irq++)
		openpic_disable_irq(irq);

#ifdef MULTIPROCESSOR
	x = openpic_read(OPENPIC_IPI_VECTOR(1));
	x &= ~(OPENPIC_IMASK | OPENPIC_PRIORITY_MASK | OPENPIC_VECTOR_MASK);
	x |= (15 << OPENPIC_PRIORITY_SHIFT) | IPI_VECTOR;
	openpic_write(OPENPIC_IPI_VECTOR(1), x);
#endif

	return pic;
}

static int
opic_irq_is_enabled(struct pic_ops *pic, int irq)
{
	/* XXX */

	return 1; 
}

static void
opic_enable_irq(struct pic_ops *pic, int irq, int type)
{

	openpic_enable_irq(irq, type);
}

static void
opic_disable_irq(struct pic_ops *pic, int irq)
{

	openpic_disable_irq(irq);
}

static void
opic_clear_irq(struct pic_ops *pic, int irq)
{

	/* we only use ack here */
}

static int
opic_get_irq(struct pic_ops *pic)
{

	return openpic_read_irq(cpu_number());
}

static void
opic_ack_irq(struct pic_ops *pic, int irq)
{

	openpic_eoi(cpu_number());
}

#endif /* PIC_OPENPIC */

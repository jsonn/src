/* $NetBSD: pic_i8259.c,v 1.1.2.1 2007/05/04 00:57:24 garbled Exp $ */

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tim Rightnour
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pic_i8259.c,v 1.1.2.1 2007/05/04 00:57:24 garbled Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <uvm/uvm_extern.h>

#include <machine/pio.h>
#include <machine/intr.h>

#include <arch/powerpc/pic/picvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

void i8259_initialize(void);
static int  i8259_irq_is_enabled(struct pic_ops *, int);
static void i8259_enable_irq(struct pic_ops *, int, int);
static void i8259_disable_irq(struct pic_ops *, int);
static void i8259_clear_irq(struct pic_ops *, int);
static int  i8259_get_irq(struct pic_ops *);
static void i8259_ack_irq(struct pic_ops *, int);

struct i8259_ops {
	struct pic_ops pic;
	uint32_t pending_events;
	uint32_t enable_mask;
	uint32_t irqs;
};

struct pic_ops *
setup_i8259(void)
{
	struct i8259_ops *i8259;
	struct pic_ops *pic;

	i8259 = malloc(sizeof(struct i8259_ops), M_DEVBUF, M_NOWAIT);
	KASSERT(i8259 != NULL);
	pic = &i8259->pic;

	pic->pic_numintrs = 16;
	pic->pic_cookie = (void *)NULL;
	pic->pic_irq_is_enabled = i8259_irq_is_enabled;
	pic->pic_enable_irq = i8259_enable_irq;
	pic->pic_reenable_irq = i8259_enable_irq;
	pic->pic_disable_irq = i8259_disable_irq;
	pic->pic_clear_irq = i8259_clear_irq;
	pic->pic_get_irq = i8259_get_irq;
	pic->pic_ack_irq = i8259_ack_irq;
	pic->pic_establish_irq = dummy_pic_establish_intr;
	strcpy(pic->pic_name, "i8259");
	pic_add(pic);
	i8259->pending_events = 0;
	i8259->enable_mask = 0xffffffff;
	i8259->irqs = 0;

	i8259_initialize();

	return pic;
}

void
i8259_initialize(void)
{
	isa_outb(IO_ICU1, 0x11);		/* program device, four bytes */
	isa_outb(IO_ICU1+1, 0);			/* starting at this vector */
	isa_outb(IO_ICU1+1, 1 << IRQ_SLAVE);	/* slave on line 2 */
	isa_outb(IO_ICU1+1, 1);			/* 8086 mode */
	isa_outb(IO_ICU1+1, 0xff);		/* leave interrupts masked */

	isa_outb(IO_ICU2, 0x11);		/* program device, four bytes */
	isa_outb(IO_ICU2+1, 8);			/* starting at this vector */
	isa_outb(IO_ICU2+1, IRQ_SLAVE);
	isa_outb(IO_ICU2+1, 1);			/* 8086 mode */
	isa_outb(IO_ICU2+1, 0xff);		/* leave interrupts masked */
}

static int
i8259_irq_is_enabled(struct pic_ops *pic, int irq)
{
	return 1;
}

static void
i8259_enable_irq(struct pic_ops *pic, int irq, int type)
{
	struct i8259_ops *i8259 = (struct i8259_ops *)pic;

	i8259->irqs |= 1 << irq;
	if (i8259->irqs >= 0x100) /* IRQS >= 8 in use? */
		i8259->irqs |= 1 << IRQ_SLAVE;

	i8259->enable_mask = ~i8259->irqs;
	isa_outb(IO_ICU1+1, i8259->enable_mask);
	isa_outb(IO_ICU2+1, i8259->enable_mask >> 8);
}

static void
i8259_disable_irq(struct pic_ops *pic, int irq)
{
	struct i8259_ops *i8259 = (struct i8259_ops *)pic;
	uint32_t mask = 1 << irq;

	i8259->enable_mask |= mask;
	isa_outb(IO_ICU1+1, i8259->enable_mask);
	isa_outb(IO_ICU2+1, i8259->enable_mask >> 8);
}

static void
i8259_clear_irq(struct pic_ops *pic, int irq)
{
	/* do nothing */
}

static int
i8259_get_irq(struct pic_ops *pic)
{
	int irq;

	isa_outb(IO_ICU1, 0x0c);
	irq = isa_inb(IO_ICU1) & 0x07;
	if (irq == IRQ_SLAVE) {
		isa_outb(IO_ICU2, 0x0c);
		irq = (isa_inb(IO_ICU2) & 0x07) + 8;
	}

	if (irq == 0)
		return 255;

	return irq;
}

static void
i8259_ack_irq(struct pic_ops *pic, int irq)
{
	if (irq < 8) {
		isa_outb(IO_ICU1, 0xe0 | irq);
	} else {
		isa_outb(IO_ICU2, 0xe0 | (irq & 7));
		isa_outb(IO_ICU1, 0xe0 | IRQ_SLAVE);
	}
}

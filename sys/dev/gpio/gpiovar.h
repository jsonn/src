/* $NetBSD: gpiovar.h,v 1.2.6.2 2005/11/10 14:04:00 skrll Exp $ */
/*	$OpenBSD: gpiovar.h,v 1.1 2004/06/03 18:08:00 grange Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DEV_GPIO_GPIOVAR_H_
#define _DEV_GPIO_GPIOVAR_H_

/* GPIO controller description */
typedef struct gpio_chipset_tag {
	void	*gp_cookie;

	int	(*gp_pin_read)(void *, int);
	void	(*gp_pin_write)(void *, int, int);
	void	(*gp_pin_ctl)(void *, int, int);
} *gpio_chipset_tag_t;

/* GPIO pin description */
typedef struct gpio_pin {
	int	pin_num;		/* number */
	int	pin_caps;		/* capabilities */
	int	pin_flags;		/* current configuration */
	int	pin_state;		/* current state */
} gpio_pin_t;

/* Attach GPIO framework to the controller */
struct gpiobus_attach_args {
	gpio_chipset_tag_t	gba_gc;		/* underlying controller */
	gpio_pin_t		*gba_pins;	/* pins array */
	int			gba_npins;	/* total number of pins */
};

int gpiobus_print(void *, const char *);

/* GPIO framework private methods */
#define gpiobus_pin_read(gc, pin) \
    ((gc)->gp_pin_read((gc)->gp_cookie, (pin)))
#define gpiobus_pin_write(gc, pin, value) \
    ((gc)->gp_pin_write((gc)->gp_cookie, (pin), (value)))
#define gpiobus_pin_ctl(gc, pin, flags) \
    ((gc)->gp_pin_ctl((gc)->gp_cookie, (pin), (flags)))


/* Attach devices connected to the GPIO pins */
struct gpio_attach_args {
	int		ga_pin;
	u_int32_t	ga_mask;
};

#endif	/* !_DEV_GPIO_GPIOVAR_H_ */

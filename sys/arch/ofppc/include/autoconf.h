/*	$NetBSD: autoconf.h,v 1.5.22.2 2007/11/15 11:43:13 yamt Exp $	*/

#ifndef _OFPPC_AUTOCONF_H_
#define _OFPPC_AUTOCONF_H_

#include <machine/bus.h>

struct confargs {
	const char	*ca_name;
	u_int		ca_node;
	int		ca_nreg;
	u_int		*ca_reg;
	int		ca_nintr;
	int		*ca_intr;

	bus_addr_t	ca_baseaddr;
	bus_space_tag_t	ca_tag;
};

extern int console_node;

#ifdef _KERNEL
void initppc(u_int, u_int, char *);
void strayintr(int);
void dumpsys(void);

void inittodr(time_t);
void resettodr(void);
void cpu_initclocks(void);
void decr_intr(struct clockframe *);
void setstatclockrate(int);
void init_interrupt(void);
void ofppc_init_comcons(void);

int ofb_cnattach(void);
#endif /* _KERNEL */

#endif /* _OFPPC_AUTOCONF_H_ */

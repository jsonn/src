/* $NetBSD: pckbdvar.h,v 1.2.4.2 2004/08/03 10:50:14 skrll Exp $ */

#include <dev/pckbport/pckbportvar.h>

int	pckbd_cnattach(pckbport_tag_t, int);
void	pckbd_hookup_bell(void (*fn)(void *, u_int, u_int, u_int, int), void *);

/*	$NetBSD: netif.h,v 1.5.100.1 2009/01/17 13:29:22 mjf Exp $	*/

#ifndef __SYS_LIBNETBOOT_NETIF_H
#define __SYS_LIBNETBOOT_NETIF_H

#include "iodesc.h"

struct netif; /* forward */

struct netif_driver {
	char	*netif_bname;
	int	(*netif_match) __P((struct netif *, void *));
	int	(*netif_probe) __P((struct netif *, void *));
	void	(*netif_init) __P((struct iodesc *, void *));
	int	(*netif_get) __P((struct iodesc *, void *, size_t, saseconds_t));
	int	(*netif_put) __P((struct iodesc *, void *, size_t));
	void	(*netif_end) __P((struct netif *));
	struct	netif_dif *netif_ifs;
	int	netif_nifs;
};

struct netif_dif {
	int		dif_unit;
	int		dif_nsel;
	struct netif_stats *dif_stats;
	void		*dif_private;
	/* the following fields are used internally by the netif layer */
	u_long		dif_used;
};

struct netif_stats {
	int	collisions;
	int	collision_error;
	int	missed;
	int	sent;
	int	received;
	int	deferred;
	int	overflow;
};

struct netif {
	struct netif_driver	*nif_driver;
	int			nif_unit;
	int			nif_sel;
	void			*nif_devdata;
};

extern struct netif_driver	*netif_drivers[];	/* machdep */
extern int			n_netif_drivers;

extern int			netif_debug;

void		netif_init __P((void));
struct netif	*netif_select __P((void *));
int		netif_probe __P((struct netif *, void *));
void		netif_attach __P((struct netif *, struct iodesc *, void *));
void		netif_detach __P((struct netif *));

int		netif_open __P((void *));
int		netif_close __P((int));

#endif /* __SYS_LIBNETBOOT_NETIF_H */

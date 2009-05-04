/*	$NetBSD: dev_net.h,v 1.5.20.1 2009/05/04 08:13:51 yamt Exp $	*/

int	net_open(struct open_file *, ...);
int	net_close(struct open_file *);
int	net_ioctl(struct open_file *, u_long, void *);
int	net_strategy(void *, int , daddr_t , size_t, void *, size_t *);

#ifdef SUPPORT_BOOTP
extern int try_bootp;
#endif

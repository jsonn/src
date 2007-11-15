/*	$NetBSD: dev_net.h,v 1.4.58.1 2007/11/15 11:44:58 yamt Exp $	*/

int	net_open __P((struct open_file *, ...));
int	net_close __P((struct open_file *));
int	net_ioctl __P((struct open_file *, u_long, void *));
int	net_strategy __P((void *, int , daddr_t , size_t, void *, size_t *));

#ifdef SUPPORT_BOOTP
extern int try_bootp;
#endif

/*	$NetBSD: wsconsio.h,v 1.1.2.3 2004/09/18 14:38:10 skrll Exp $	*/

#ifndef _NEWSMIPS_WSCONSIO_H_
#define	_NEWSMIPS_WSCONSIO_H_

#include <dev/wscons/wsconsio.h>

struct newsmips_wsdisplay_fbinfo {
	struct wsdisplay_fbinfo wsdisplay_fbinfo;
	u_int stride;
};

#define	NEWSMIPS_WSDISPLAYIO_GINFO						\
	_IOR('n', 65, struct newsmips_wsdisplay_fbinfo)

#endif /* !_NEWSMIPS_WSCONSIO_H_ */

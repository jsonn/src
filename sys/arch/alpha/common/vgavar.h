/*	$NetBSD: vgavar.h,v 1.2.2.1 1996/12/07 02:04:53 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

struct vga_config {
	bus_space_tag_t	vc_iot, vc_memt;
	bus_space_handle_t vc_ioh_b, vc_ioh_c, vc_ioh_d, vc_memh;

	u_int	vc_type;		/* type (for wsdisplay) */
	u_int	vc_crow, vc_ccol;	/* current cursor position */

	char	vc_so;			/* in standout mode? */
	char	vc_at;			/* normal attributes */
	char	vc_so_at;		/* standout attributes */
};

int	vga_common_probe __P((bus_space_tag_t, bus_space_tag_t));
void	vga_common_setup __P((bus_space_tag_t, bus_space_tag_t, u_int,
	    struct vga_config *));
void	vga_wsdisplay_attach __P((struct device *, struct vga_config *, int));
void	vga_wsdisplay_console __P((struct vga_config *));

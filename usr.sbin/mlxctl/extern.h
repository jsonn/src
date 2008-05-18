/*	$NetBSD: extern.h,v 1.3.30.1 2008/05/18 12:36:20 yamt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * main.c
 */
extern int	mlxfd;
extern const char	*mlxname;
extern const char	*nlistf;
extern const char	*memf;
extern int	verbosity;
extern struct mlx_cinfo	ci;

void	usage(void) __attribute__ ((__noreturn__));

/*
 * dklist.c
 */
struct mlx_disk {
	SIMPLEQ_ENTRY(mlx_disk) chain;
	int	hwunit;
	char	name[8];
};

void	mlx_disk_init(void);
int	mlx_disk_empty(void);
void	mlx_disk_add(const char *);
void	mlx_disk_add_all(void);
void	mlx_disk_iterate(void (*)(struct mlx_disk *));

/*
 * util.c
 */
int	mlx_command(struct mlx_usercommand *, int);
void	mlx_enquiry(struct mlx_enquiry2 *enq);
void	mlx_configuration(struct mlx_core_cfg *, int);
int	mlx_scsi_inquiry(int, int, char **, char **, char **);
int	mlx_get_device_state(int, int, struct mlx_phys_drv *);
void	mlx_print_phys_drv(struct mlx_phys_drv *, int, int, const char *);

/*
 * cmds.c
 */
int	cmd_check(char **);
int	cmd_cstatus(char **);
int	cmd_detach(char **);
int	cmd_rebuild(char **);
int	cmd_rescan(char **);
int	cmd_status(char **);

/*
 * config.c
 */
int	cmd_config(char **);

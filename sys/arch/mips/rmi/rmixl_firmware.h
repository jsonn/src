/*	$NetBSD: rmixl_firmware.h,v 1.1.2.2 2009/11/09 10:02:48 cliff Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Cliff Neighbors
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

/*********************************************************************

  Copyright 2003-2006 Raza Microelectronics, Inc. (RMI). All rights
  reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.

  THIS SOFTWARE IS PROVIDED BY Raza Microelectronics, Inc. ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RMI OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.

  *****************************#RMI_2#**********************************/

#ifndef _ARCH_MIPS_RMI_RMIXL_FIRMWARE_H_
#define _ARCH_MIPS_RMI_RMIXL_FIRMWARE_H_

typedef struct rmixlfw_info {
	uint64_t boot_level;
	uint64_t io_base;
	uint64_t output_device;
	uint64_t uart_print;
	uint64_t led_output;
	uint64_t init;
	uint64_t exit;
	uint64_t warm_reset;
	uint64_t wakeup;
	uint64_t cpu_online_map;
	uint64_t master_reentry_sp;
	uint64_t master_reentry_gp;
	uint64_t master_reentry_fn;
	uint64_t slave_reentry_fn;
	uint64_t magic_dword;
	uint64_t uart_putchar;
	uint64_t size;
	uint64_t uart_getchar;
	uint64_t nmi_handler;
	uint64_t psb_version;
	uint64_t mac_addr;
	uint64_t cpu_frequency;
	uint64_t board_version;
	uint64_t malloc;
	uint64_t free;
	uint64_t global_shmem_addr;
	uint64_t global_shmem_size;
	uint64_t psb_os_cpu_map;
	uint64_t userapp_cpu_map;
	uint64_t wakeup_os;
	uint64_t psb_mem_map;
	uint64_t board_major_version;
	uint64_t board_minor_version;
	uint64_t board_manf_revision;
	uint64_t board_serial_number;
	uint64_t psb_physaddr_map;
	uint64_t xlr_loaderip_config;
	uint64_t bldr_envp;
	uint64_t avail_mem_map;
} rmixlfw_info_t;


#define RMIXLFW_MMAP_MAX_MMAPS		32

#define RMIXLFW_MMAP_TYPE_RAM		1
#define RMIXLFW_MMAP_TYPE_ROM		2
#define RMIXLFW_MMAP_TYPE_RESERVED	3
#define RMIXLFW_MMAP_TYPE_DEV_IO	0x10
#define RMIXLFW_MMAP_TYPE_PCI_IO	0x11
#define RMIXLFW_MMAP_TYPE_PCI_CFG	0x12
#define RMIXLFW_MMAP_TYPE_PCI_MEM	0x13
#define RMIXLFW_MMAP_TYPE_UNKNOWN	0xff

/*
 * struct at psb_mem_map, psb_physaddr_map, avail_mem_map
 */
typedef struct rmixlfw_mmap {
	uint32_t nmmaps;
	struct rmixlfw_mmap_entry {
		uint64_t start;
		uint64_t size;
		uint32_t type;
	} entry[RMIXLFW_MMAP_MAX_MMAPS];
} rmixlfw_mmap_t;

#endif	/* _ARCH_MIPS_RMI_RMIXL_FIRMWARE_H_ */

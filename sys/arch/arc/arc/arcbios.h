/*	$NetBSD: arcbios.h,v 1.4.8.2 2001/06/13 15:08:06 soda Exp $	*/
/*	$OpenBSD: arcbios.h,v 1.1 1998/01/29 15:06:22 pefo Exp $	*/

/*-
 * Copyright (c) 1996 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

typedef struct arc_sid
{
	char vendor[8];
	char prodid[8];
} arc_sid_t;

typedef enum arc_config_class
{
	arc_SystemClass,
	arc_ProcessorClass,
	arc_CacheClass,
	arc_AdapterClass,
	arc_ControllerClass,
	arc_PeripheralClass,
	arc_MemoryClass
} arc_config_class_t;

typedef enum arc_config_type
{
	arc_System,

	arc_CentralProcessor,
	arc_FloatingPointProcessor,

	arc_PrimaryIcache,
	arc_PrimaryDcache,
	arc_SecondaryIcache,
	arc_SecondaryDcache,
	arc_SecondaryCache,

	arc_EisaAdapter,		/* Eisa adapter         */
	arc_TcAdapter,			/* Turbochannel adapter */
	arc_ScsiAdapter,		/* SCSI adapter         */
	arc_DtiAdapter,			/* AccessBus adapter    */
	arc_MultiFunctionAdapter,

	arc_DiskController,
	arc_TapeController,
	arc_CdromController,
	arc_WormController,
	arc_SerialController,
	arc_NetworkController,
	arc_DisplayController,
	arc_ParallelController,
	arc_PointerController,
	arc_KeyboardController,
	arc_AudioController,
	arc_OtherController,		/* denotes a controller not otherwise defined */

	arc_DiskPeripheral,
	arc_FloppyDiskPeripheral,
	arc_TapePeripheral,
	arc_ModemPeripheral,
	arc_MonitorPeripheral,
	arc_PrinterPeripheral,
	arc_PointerPeripheral,
	arc_KeyboardPeripheral,
	arc_TerminalPeripheral,
	arc_OtherPeripheral,		/* denotes a peripheral not otherwise defined   */
	arc_LinePeripheral,
	arc_NetworkPeripheral,

	arc_SystemMemory
} arc_config_type_t;

typedef u_char arc_dev_flags_t;

/* Wonder how this is aligned... */
typedef struct arc_config
{
	arc_config_class_t	class;		/* Likely these three all */
	arc_config_type_t	type;		/* need to be uchar to make */
	arc_dev_flags_t		flags;		/* the alignment right */
	u_int16_t		version;
	u_int16_t		revision;
	u_int32_t		key;
	u_int32_t		affinity_mask;
	u_int32_t		config_data_len;
	u_int32_t		id_len;
	char 			*id;
} arc_config_t;

typedef enum arc_status
{
	arc_ESUCCESS,			/* Success                   */
	arc_E2BIG,			/* Arg list too long         */
	arc_EACCES,			/* No such file or directory */
	arc_EAGAIN,			/* Try again                 */
	arc_EBADF,			/* Bad file number           */
	arc_EBUSY,			/* Device or resource busy   */
	arc_EFAULT,			/* Bad address               */
	arc_EINVAL,			/* Invalid argument          */
	arc_EIO,			/* I/O error                 */
	arc_EISDIR,			/* Is a directory            */
	arc_EMFILE,			/* Too many open files       */
	arc_EMLINK,			/* Too many links            */
	arc_ENAMETOOLONG,		/* File name too long        */
	arc_ENODEV,			/* No such device            */
	arc_ENOENT,			/* No such file or directory */
	arc_ENOEXEC,			/* Exec format error         */
	arc_ENOMEM,			/* Out of memory             */
	arc_ENOSPC,			/* No space left on device   */
	arc_ENOTDIR,			/* Not a directory           */
	arc_ENOTTY,			/* Not a typewriter          */
	arc_ENXIO,			/* No such device or address */
	arc_EROFS,			/* Read-only file system     */
} arc_status_t;

#ifdef arc
typedef enum {
	ExeceptionBlock,
	SystemParameterBlock,
	FreeMemory,
	BadMemory,
	LoadedProgram,
	FirmwareTemporary,
	FirmwarePermanent,
	FreeContigous
} MEMORYTYPE;
#endif
#ifdef sgi /* note: SGI's systems have different order of types. */
  typedef enum {
	ExeceptionBlock,
	SystemParameterBlock,
	FreeContigous,
	FreeMemory,
	BadMemory,
	LoadedProgram,
	FirmwareTemporary,
	FirmwarePermanent,
  } MEMORYTYPE;
#endif

typedef struct arc_mem {
	MEMORYTYPE	Type;		/* Memory chunk type */
	u_int32_t	BasePage;	/* Page no, first page */
	u_int32_t	PageCount;	/* Number of pages */
} arc_mem_t;

typedef caddr_t arc_time_t; /* XXX */

typedef struct arc_dsp_stat {
	u_int16_t	CursorXPosition;
	u_int16_t	CursorYPosition;
	u_int16_t	CursorMaxXPosition;
	u_int16_t	CursorMaxYPosition;
	u_char		ForegroundColor;
	u_char		BackgroundColor;
	u_char		HighIntensity;
	u_char		Underscored;
	u_char		ReverseVideo;
} arc_dsp_stat_t;

typedef caddr_t arc_dirent_t; /* XXX */
typedef u_int32_t arc_open_mode_t; /* XXX */
typedef u_int32_t arc_seek_mode_t; /* XXX */
typedef u_int32_t arc_mount_t; /* XXX */

typedef struct arc_calls
{
	arc_status_t (*load)(		/* Load 1 */
		char *,			/* Image to load */
		u_int32_t,		/* top address */
		u_int32_t *,		/* Entry address */
		u_int32_t *);		/* Low address */

	arc_status_t (*invoke)(		/* Invoke 2 */
		u_int32_t,		/* Entry Address */
		u_int32_t,		/* Stack Address */
		u_int32_t,		/* Argc */
		char **,		/* argv */
		char **);		/* envp */

	arc_status_t (*execute)(	/* Execute 3 */
		char *,			/* Image path */
		u_int32_t,		/* Argc */
		char **,		/* argv */
		char **);		/* envp */

	volatile void (*halt)(void);	/* Halt 4 */

	volatile void (*power_down)(void); /* PowerDown 5 */

	volatile void (*restart)(void);	/* Restart 6 */

	volatile void (*reboot)(void);	/* Reboot 7 */

	volatile void (*enter_interactive_mode)(void); /* EnterInteractiveMode 8 */

	volatile void (*return_from_main)(void); /* ReturnFromMain 9 */

	arc_config_t *(*get_peer)(	/* GetPeer 10 */
		arc_config_t *); 	/* Component */

	arc_config_t *(*get_child)(	/* GetChild 11 */
		arc_config_t *);	/* Component */

	arc_config_t *(*get_parent)(	/* GetParent 12 */
		arc_config_t *);	/* Component */

	arc_status_t (*get_config_data)( /* GetConfigurationData 13 */
		caddr_t,		/* Configuration Data */
		arc_config_t *);	/* Component */

	arc_config_t *(*add_child)(	/* AddChild 14 */
		arc_config_t *,		/* Component */
		arc_config_t *);	/* New Component */

	arc_status_t (*delete_component)( /* DeleteComponent 15 */
		arc_config_t *);	/* Component */

	arc_config_t *(*get_component)( /* GetComponent 16 */
		char *);		/* Path */

	arc_status_t (*save_config)(void); /* SaveConfiguration 17 */

	arc_sid_t *(*get_system_id)(void); /* GetSystemId 18 */

	arc_mem_t *(*get_memory_descriptor)( /* GetMemoryDescriptor 19 */
		arc_mem_t *);		/* MemoryDescriptor */

#ifdef arc
	void (*signal)(			/* Signal 20 */
		u_int32_t,		/* Signal number */
/**/		caddr_t);		/* Handler */
#endif
#ifdef sgi
	void *unused;
#endif

	arc_time_t *(*get_time)(void);	/* GetTime 21 */

	u_int32_t (*get_relative_time)(void); /* GetRelativeTime 22 */

	arc_status_t (*get_dir_entry)(	/* GetDirectoryEntry 23 */
		u_int32_t,		/* FileId */
		arc_dirent_t *,		/* Directory entry */
		u_int32_t,		/* Length */
		u_int32_t *);		/* Count */

	arc_status_t (*open)(		/* Open 24 */
		char *,			/* Path */
		arc_open_mode_t,	/* Open mode */
		u_int32_t *);		/* FileId */

	arc_status_t (*close)(		/* Close 25 */
		u_int32_t);		/* FileId */

	arc_status_t (*read)(		/* Read 26 */
		u_int32_t,		/* FileId */
		caddr_t,		/* Buffer */
		u_int32_t,		/* Length */
		u_int32_t *);		/* Count */

	arc_status_t (*get_read_status)( /* GetReadStatus 27 */
		u_int32_t);		/* FileId */

	arc_status_t (*write)(		/* Write 28 */
		u_int32_t,		/* FileId */
		caddr_t,		/* Buffer */
		u_int32_t,		/* Length */
		u_int32_t *);		/* Count */

	arc_status_t (*seek)(		/* Seek 29 */
		u_int32_t,		/* FileId */
		int64_t *,		/* Offset */
		arc_seek_mode_t); 	/* Mode */

	arc_status_t (*mount)(		/* Mount 30 */
		char *,			/* Path */
		arc_mount_t);		/* Operation */

	char *(*getenv)(			/* GetEnvironmentVariable 31 */
		char *);		/* Variable */

	arc_status_t (*putenv)(		/* SetEnvironmentVariable 32 */
		char *,			/* Variable */
		char *);		/* Value */

	arc_status_t (*get_file_info)(void);	/* GetFileInformation 33 */

	arc_status_t (*set_file_info)(void);	/* SetFileInformation 34 */

	void (*flush_all_caches)(void);	/* FlushAllCaches 35 */

	/* note: the followings don't exist on SGI */
#ifdef arc
	arc_status_t (*test_unicode)(	/* TestUnicodeCharacter 36 */
		u_int32_t,		/* FileId */
		u_int16_t);		/* UnicodeCharacter */

	arc_dsp_stat_t *(*get_display_status)( /* GetDisplayStatus 37 */
		u_int32_t);		/* FileId */
#endif
} arc_calls_t;

#define ARC_PARAM_BLK_MAGIC	0x53435241
#define ARC_PARAM_BLK_MAGIC_BUG	0x41524353	/* This is wrong... but req */

typedef struct arc_param_blk 
{
	u_int32_t	magic;		/* Magic Number */
	u_int32_t	length;		/* Length of parameter block */
	u_int16_t	version;	/* ?? */
	u_int16_t	revision;	/* ?? */
/**/	caddr_t		restart_block;	/* ?? */
/**/	caddr_t		debug_block;	/* Debugging info -- unused */
/**/	caddr_t		general_exp_vect; /* ?? */
/**/	caddr_t		tlb_miss_exp_vect; /* ?? */
	u_int32_t	firmware_length; /* Size of Firmware jumptable in bytes */
	arc_calls_t	*firmware_vect;	/* Firmware jumptable */
	u_int32_t	vendor_length;	/* Size of Vendor specific jumptable */
/**/	caddr_t		vendor_vect;	/* Vendor specific jumptable */
	u_int32_t	adapter_count;	/* ?? */
	u_int32_t	adapter0_type;	/* ?? */
	u_int32_t	adapter0_length; /* ?? */
/**/	caddr_t		adapter0_vect;	/* ?? */
} arc_param_blk_t;

#define ArcBiosBase ((arc_param_blk_t *) 0x80001000)
#define ArcBios (ArcBiosBase->firmware_vect)

/*
 * All functions except bios_ident() should be called only if
 *	bios_ident() returns >= 0.
 */
int bios_ident __P((void));
void bios_init_console __P((void));
int bios_configure_memory __P((int *, phys_ram_seg_t *, int *));
void bios_save_info __P((void));
#ifdef arc
void bios_display_info __P((int *, int *, int *, int *));
#endif

extern char arc_vendor_id[sizeof(((arc_sid_t *)0)->vendor) + 1];
extern unsigned char arc_product_id[sizeof(((arc_sid_t *)0)->prodid)];
extern char arc_id[64 + 1];
extern char arc_displayc_id[64 + 1];
extern arc_dsp_stat_t arc_displayinfo;
extern int arc_cpu_l2cache_size;

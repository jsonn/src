/*	$NetBSD: aacreg.h,v 1.1.14.1 2005/02/15 21:33:12 skrll Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 Scott Long
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2000 Niklas Hallqvist
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from FreeBSD: aacreg.h,v 1.1 2000/09/13 03:20:34 msmith Exp
 * via OpenBSD: aacreg.h,v 1.3 2001/06/12 15:40:29 niklas Exp
 */

/*
 * Data structures defining the interface between the driver and the Adaptec
 * 'FSA' adapters.  Note that many field names and comments here are taken
 * verbatim from the Adaptec driver source in order to make comparing the
 * two slightly easier.
 */

#ifndef _PCI_AACREG_H_
#define	_PCI_AACREG_H_

/*
 * Misc. magic numbers.
 */
#define	AAC_MAX_CONTAINERS	64
#define	AAC_BLOCK_SIZE		512

/*
 * Communications interface.
 *
 * Where datastructure layouts are closely parallel to the Adaptec sample code,
 * retain their naming conventions (for now) to aid in cross-referencing.
 */

/*
 * We establish 4 command queues and matching response queues.  Queues must
 * be 16-byte aligned, and are sized as follows:
 */
#define	AAC_HOST_NORM_CMD_ENTRIES	8	/* cmd adapter->host, normal pri */
#define	AAC_HOST_HIGH_CMD_ENTRIES	4	/* cmd adapter->host, high pri */
#define	AAC_ADAP_NORM_CMD_ENTRIES	512	/* cmd host->adapter, normal pri */
#define	AAC_ADAP_HIGH_CMD_ENTRIES	4	/* cmd host->adapter, high pri */
#define	AAC_HOST_NORM_RESP_ENTRIES	512	/* resp, adapter->host, normal pri */
#define	AAC_HOST_HIGH_RESP_ENTRIES	4	/* resp, adapter->host, high pri */
#define	AAC_ADAP_NORM_RESP_ENTRIES	8	/* resp, host->adapter, normal pri */
#define	AAC_ADAP_HIGH_RESP_ENTRIES	4	/* resp, host->adapter, high pri */

#define	AAC_TOTALQ_LENGTH \
    (AAC_HOST_HIGH_CMD_ENTRIES + AAC_HOST_NORM_CMD_ENTRIES + \
    AAC_ADAP_HIGH_CMD_ENTRIES +	AAC_ADAP_NORM_CMD_ENTRIES + \
    AAC_HOST_HIGH_RESP_ENTRIES + AAC_HOST_NORM_RESP_ENTRIES + \
    AAC_ADAP_HIGH_RESP_ENTRIES + AAC_ADAP_NORM_RESP_ENTRIES)

#define	AAC_QUEUE_COUNT		8
#define	AAC_QUEUE_ALIGN		16

struct aac_queue_entry {
	u_int32_t aq_fib_size;		/* FIB size in bytes */
	u_int32_t aq_fib_addr;		/* receiver-space address of the FIB */
} __attribute__ ((__packed__));

#define	AAC_PRODUCER_INDEX	0
#define	AAC_CONSUMER_INDEX	1

/*
 * Table of queue indices and queues used to communicate with the
 * controller.  This structure must be aligned to AAC_QUEUE_ALIGN
 */
struct aac_queue_table {
	/* queue consumer/producer indexes (layout mandated by adapter) */
	u_int32_t qt_qindex[AAC_QUEUE_COUNT][2];

	/* queue entry structures (layout mandated by adapter) */
	struct aac_queue_entry qt_HostNormCmdQueue[AAC_HOST_NORM_CMD_ENTRIES];
	struct aac_queue_entry qt_HostHighCmdQueue[AAC_HOST_HIGH_CMD_ENTRIES];
	struct aac_queue_entry qt_AdapNormCmdQueue[AAC_ADAP_NORM_CMD_ENTRIES];
	struct aac_queue_entry qt_AdapHighCmdQueue[AAC_ADAP_HIGH_CMD_ENTRIES];
	struct aac_queue_entry
	    qt_HostNormRespQueue[AAC_HOST_NORM_RESP_ENTRIES];
	struct aac_queue_entry
	    qt_HostHighRespQueue[AAC_HOST_HIGH_RESP_ENTRIES];
	struct aac_queue_entry
	    qt_AdapNormRespQueue[AAC_ADAP_NORM_RESP_ENTRIES];
	struct aac_queue_entry
	    qt_AdapHighRespQueue[AAC_ADAP_HIGH_RESP_ENTRIES];
} __attribute__ ((__packed__));

/*
 * Adapter Init Structure: this is passed to the adapter with the 
 * AAC_MONKER_INITSTRUCT command to point it at our control structures.
 */
struct aac_adapter_init {
	u_int32_t InitStructRevision;
#define	AAC_INIT_STRUCT_REVISION	3
	u_int32_t MiniPortRevision;
	u_int32_t FilesystemRevision;
	u_int32_t CommHeaderAddress;
	u_int32_t FastIoCommAreaAddress;
	u_int32_t AdapterFibsPhysicalAddress;
	void	*AdapterFibsVirtualAddress;
	u_int32_t AdapterFibsSize;
	u_int32_t AdapterFibAlign;
	u_int32_t PrintfBufferAddress;
	u_int32_t PrintfBufferSize;
	u_int32_t HostPhysMemPages;
	u_int32_t HostElapsedSeconds;
} __attribute__((__packed__));

/*
 * Shared data types
 */

/*
 * Container types
 */
#define	CT_NONE			0
#define	CT_VOLUME		1
#define	CT_MIRROR		2
#define	CT_STRIPE		3
#define	CT_RAID5		4
#define	CT_SSRW			5
#define	CT_SSRO			6
#define	CT_MORPH		7
#define	CT_PASSTHRU		8
#define	CT_RAID4		9
#define	CT_RAID10		10	/* stripe of mirror */
#define	CT_RAID00		11	/* stripe of stripe */
#define	CT_VOLUME_OF_MIRRORS	12	/* volume of mirror */
#define	CT_PSEUDO_RAID3		13	/* really raid4 */

/*
 * Host-addressable object types
 */
#define	FT_REG			1	/* regular file */
#define	FT_DIR			2	/* directory */
#define	FT_BLK			3	/* "block" device - reserved */
#define	FT_CHR			4	/* "character special" device - reserved */
#define	FT_LNK			5	/* symbolic link */
#define	FT_SOCK			6	/* socket */
#define	FT_FIFO			7	/* fifo */
#define	FT_FILESYS		8	/* ADAPTEC's "FSA"(tm) filesystem */
#define	FT_DRIVE		9	/* phys disk - addressable in scsi by bus/target/lun */
#define	FT_SLICE		10	/* virtual disk - raw volume - slice */
#define	FT_PARTITION		11	/* FSA part, inside slice, container building block */
#define	FT_VOLUME		12	/* Container - Volume Set */
#define	FT_STRIPE		13	/* Container - Stripe Set */
#define	FT_MIRROR		14	/* Container - Mirror Set */
#define	FT_RAID5		15	/* Container - Raid 5 Set */
#define	FT_DATABASE		16	/* Storage object with "foreign" content manager */

/*
 * Host-side scatter/gather list for 32-bit commands.
 */
struct aac_sg_entry {
	u_int32_t SgAddress;
	u_int32_t SgByteCount;
} __attribute__ ((__packed__));

struct aac_sg_table {
	u_int32_t SgCount;
	struct aac_sg_entry SgEntry[0];	/* XXX */
} __attribute__ ((__packed__));

/*
 * Host-side scatter/gather list for 64-bit commands.
 */
struct aac_sg_table64 {
	u_int8_t SgCount;
	u_int8_t SgSectorsPerPage;
	u_int16_t SgByteOffset;
	u_int64_t SgEntry[0];
} __attribute__ ((__packed__));

/*
 * Container creation data
 */
struct aac_container_creation {
	u_int8_t ViaBuildNumber;
	u_int8_t MicroSecond;
	u_int8_t Via;			/* 1 = FSU, 2 = API, etc. */
	u_int8_t YearsSince1900;
	u_int32_t Month:4;		/* 1-12 */
	u_int32_t Day:6;		/* 1-32 */
	u_int32_t Hour:6;		/* 0-23 */
	u_int32_t Minute:6;		/* 0-59 */
	u_int32_t Second:6;		/* 0-59 */
	u_int64_t ViaAdapterSerialNumber;
} __attribute__ ((__packed__));

struct FsaRevision {
	union {
        	struct {
			u_int8_t dash;
			u_int8_t type;
			u_int8_t minor;
			u_int8_t major;
	        } comp;
	        u_int32_t ul;
	} external;
	u_int32_t buildNumber;
} __attribute__((__packed__));

/*
 * Adapter Information
 */

#define	CPU_NTSIM		1
#define	CPU_I960		2
#define	CPU_ARM			3
#define	CPU_SPARC		4
#define	CPU_POWERPC		5
#define	CPU_ALPHA		6
#define	CPU_P7			7
#define	CPU_I960_RX		8
#define	CPU__last		9

#define	CPUI960_JX		1
#define	CPUI960_CX		2
#define	CPUI960_HX		3
#define	CPUI960_RX		4
#define	CPUARM_SA110		5
#define	CPUARM_xxx		6
#define	CPUPPC_603e		7
#define	CPUPPC_xxx		8
#define	CPUI80303		9
#define	CPUSUBTYPE__last	10

#define	PLAT_NTSIM		1
#define	PLAT_V3ADU		2
#define	PLAT_CYCLONE		3
#define	PLAT_CYCLONE_HD		4
#define	PLAT_BATBOARD		5
#define	PLAT_BATBOARD_HD	6
#define	PLAT_YOLO		7
#define	PLAT_COBRA		8
#define	PLAT_ANAHEIM		9
#define	PLAT_JALAPENO		10
#define	PLAT_QUEENS		11
#define	PLAT_JALAPENO_DELL	12
#define	PLAT_POBLANO		13
#define	PLAT_POBLANO_OPAL	14
#define	PLAT_POBLANO_SL0	15
#define	PLAT_POBLANO_SL1	16
#define	PLAT_POBLANO_SL2	17
#define	PLAT_POBLANO_XXX	18
#define	PLAT_JALAPENO_P2	19
#define	PLAT_HABANERO		20
#define	PLAT__last		21

#define	OEM_FLAVOR_ADAPTEC	1
#define	OEM_FLAVOR_DELL		2
#define	OEM_FLAVOR_HP		3
#define	OEM_FLAVOR_IBM		4
#define	OEM_FLAVOR_CPQ		5
#define	OEM_FLAVOR_BRAND_X	6
#define	OEM_FLAVOR_BRAND_Y	7
#define	OEM_FLAVOR_BRAND_Z	8
#define	OEM_FLAVOR__last	9

/*
 * XXX the aac-2622 with no battery present reports PLATFORM_BAT_OPT_PRESENT
 */
#define	PLATFORM_BAT_REQ_PRESENT	1	/* BATTERY REQUIRED AND PRESENT */
#define	PLATFORM_BAT_REQ_NOTPRESENT	2	/* BATTERY REQUIRED AND NOT PRESENT */
#define	PLATFORM_BAT_OPT_PRESENT	3	/* BATTERY OPTIONAL AND PRESENT */
#define	PLATFORM_BAT_OPT_NOTPRESENT	4	/* BATTERY OPTIONAL AND NOT PRESENT */
#define	PLATFORM_BAT_NOT_SUPPORTED	5	/* BATTERY NOT SUPPORTED */

/* 
 * Structure used to respond to a RequestAdapterInfo fib.
 */
struct aac_adapter_info {
	u_int32_t PlatformBase;		/* adapter type */
	u_int32_t CpuArchitecture;	/* adapter CPU type */
	u_int32_t CpuVariant;		/* adapter CPU subtype */
	u_int32_t ClockSpeed;		/* adapter CPU clockspeed */
	u_int32_t ExecutionMem;		/* adapter Execution Memory size */
	u_int32_t BufferMem;		/* adapter Data Memory */
	u_int32_t TotalMem;		/* adapter Total Memory */
	struct FsaRevision KernelRevision; /* adapter Kernel SW Revision */
	struct FsaRevision MonitorRevision; /* adapter Monitor/Diag SW Rev */
	struct FsaRevision HardwareRevision; /* TDB */
	struct FsaRevision BIOSRevision; /* adapter BIOS Revision */
	u_int32_t ClusteringEnabled;
	u_int32_t ClusterChannelMask;
	u_int64_t SerialNumber;
	u_int32_t batteryPlatform;
	u_int32_t SupportedOptions;	/* supported features of this ctrlr */
	u_int32_t OemVariant;
} __attribute__((__packed__));

/*
 * Monitor/Kernel interface.
 */

/*
 * Synchronous commands to the monitor/kernel.
 */
#define	AAC_MONKER_INITSTRUCT	0x05
#define	AAC_MONKER_SYNCFIB	0x0c
#define	AAC_MONKER_GETKERNVER	0x11

/*
 * Command status values
 */
#define	ST_OK			0
#define	ST_PERM			1
#define	ST_NOENT		2
#define	ST_IO			5
#define	ST_NXIO			6
#define	ST_E2BIG		7
#define	ST_ACCES		13
#define	ST_EXIST		17
#define	ST_XDEV			18
#define	ST_NODEV		19
#define	ST_NOTDIR		20
#define	ST_ISDIR		21
#define	ST_INVAL		22
#define	ST_FBIG			27
#define	ST_NOSPC		28
#define	ST_ROFS			30
#define	ST_MLINK		31
#define	ST_WOULDBLOCK		35
#define	ST_NAMETOOLONG		63
#define	ST_NOTEMPTY		66
#define	ST_DQUOT		69
#define	ST_STALE		70
#define	ST_REMOTE		71
#define	ST_BADHANDLE		10001
#define	ST_NOT_SYNC		10002
#define	ST_BAD_COOKIE		10003
#define	ST_NOTSUPP		10004
#define	ST_TOOSMALL		10005
#define	ST_SERVERFAULT		10006
#define	ST_BADTYPE		10007
#define	ST_JUKEBOX		10008
#define	ST_NOTMOUNTED		10009
#define	ST_MAINTMODE		10010
#define	ST_STALEACL		10011

/*
 * Volume manager commands
 */
#define	VM_Null			0
#define	VM_NameServe		1
#define	VM_ContainerConfig	2
#define	VM_Ioctl		3
#define	VM_FilesystemIoctl	4
#define	VM_CloseAll		5
#define	VM_CtBlockRead		6
#define	VM_CtBlockWrite		7
#define	VM_SliceBlockRead	8	/* raw access to configured "storage objects" */
#define	VM_SliceBlockWrite	9
#define	VM_DriveBlockRead	10	/* raw access to physical devices */
#define	VM_DriveBlockWrite	11
#define	VM_EnclosureMgt		12	/* enclosure management */
#define	VM_Unused		13	/* used to be diskset management */
#define	VM_CtBlockVerify	14
#define	VM_CtPerf		15	/* performance test */
#define	VM_CtBlockRead64	16
#define	VM_CtBlockWrite64	17
#define	VM_CtBlockVerify64	18

/*
 * "Mountable object"
 */
struct aac_mntobj {
	u_int32_t ObjectId;
	char	FileSystemName[16];
	struct aac_container_creation CreateInfo;
	u_int32_t Capacity;
	u_int32_t VolType;
	u_int32_t ObjType;
	u_int32_t ContentState;
#define	AAC_FSCS_READONLY 0x0002 /* XXX need more information than this */
	union {
		u_int32_t pad[8];
	} ObjExtension;
	u_int32_t AlterEgoId;
} __attribute__ ((__packed__));

struct aac_mntinfo {
	u_int32_t Command;
	u_int32_t MntType;
	u_int32_t MntCount;
} __attribute__ ((__packed__));

struct aac_mntinforesponse {
	u_int32_t Status;
	u_int32_t MntType;
	u_int32_t MntRespCount;
	struct aac_mntobj MntTable[1];
} __attribute__ ((__packed__));

/*
 * Write 'stability' options.
 */
#define	CSTABLE			1
#define	CUNSTABLE		2

/*
 * Commit level response for a write request.
 */
#define	CMFILE_SYNC_NVRAM	1
#define	CMDATA_SYNC_NVRAM	2
#define	CMFILE_SYNC		3
#define	CMDATA_SYNC		4
#define	CMUNSTABLE		5

/*
 * Block read/write operations.  These structures are packed into the 'data'
 * area in the FIB.
 */
struct aac_blockread {
	u_int32_t Command;		/* not FSACommand! */
	u_int32_t ContainerId;
	u_int32_t BlockNumber;
	u_int32_t ByteCount;
	struct aac_sg_table SgMap;	/* variable size */
} __attribute__ ((__packed__));

struct aac_blockread_response {
	u_int32_t Status;
	u_int32_t ByteCount;
} __attribute__ ((__packed__));

struct aac_blockwrite {
	u_int32_t Command;	/* not FSACommand! */
	u_int32_t ContainerId;
	u_int32_t BlockNumber;
	u_int32_t ByteCount;
	u_int32_t Stable;
	struct aac_sg_table SgMap;	/* variable size */
} __attribute__ ((__packed__));

struct aac_blockwrite_response {
	u_int32_t Status;
	u_int32_t ByteCount;
	u_int32_t Committed;
} __attribute__ ((__packed__));

struct aac_close_command {
	u_int32_t	Command;
	u_int32_t	ContainerId;
} __attribute__ ((__packed__));

/*
 * Register definitions for the Adaptec AAC-364 'Jalapeno I/II' adapters, based
 * on the SA110 'StrongArm'.
 */

#define	AAC_REGSIZE		0x100

/* doorbell 0 (adapter->host) */
#define	AAC_SA_DOORBELL0_CLEAR	0x98
#define	AAC_SA_DOORBELL0_SET	0x9c
#define	AAC_SA_DOORBELL0	0x9c
#define	AAC_SA_MASK0_CLEAR	0xa0
#define	AAC_SA_MASK0_SET	0xa4

/* doorbell 1 (host->adapter) */
#define	AAC_SA_DOORBELL1_CLEAR	0x9a
#define	AAC_SA_DOORBELL1_SET	0x9e
#define	AAC_SA_MASK1_CLEAR	0xa2
#define	AAC_SA_MASK1_SET	0xa6

/* mailbox (20 bytes) */
#define	AAC_SA_MAILBOX		0xa8
#define	AAC_SA_FWSTATUS		0xc4

/*
 * Register definitions for the Adaptec 'Pablano' adapters, based on the
 * i960Rx, and other related adapters.
 */

#define	AAC_RX_IDBR		0x20	/* inbound doorbell */
#define	AAC_RX_IISR		0x24	/* inbound interrupt status */
#define	AAC_RX_IIMR		0x28	/* inbound interrupt mask */
#define	AAC_RX_ODBR		0x2c	/* outbound doorbell */
#define	AAC_RX_OISR		0x30	/* outbound interrupt status */
#define	AAC_RX_OIMR		0x34	/* outbound interrupt mask */

#define	AAC_RX_MAILBOX		0x50	/* mailbox (20 bytes) */
#define	AAC_RX_FWSTATUS		0x6c

/*
 * Common bit definitions for the doorbell registers.
 */

/*
 * Status bits in the doorbell registers.
 */
#define	AAC_DB_SYNC_COMMAND	(1<<0)	/* send/completed synchronous FIB */
#define	AAC_DB_COMMAND_READY	(1<<1)	/* posted one or more commands */
#define	AAC_DB_RESPONSE_READY	(1<<2)	/* one or more commands complete */
#define	AAC_DB_COMMAND_NOT_FULL	(1<<3)	/* command queue not full */
#define	AAC_DB_RESPONSE_NOT_FULL (1<<4)	/* response queue not full */

/*
 * The adapter can request the host print a message by setting the
 * DB_PRINTF flag in DOORBELL0.  The driver responds by collecting the
 * message from the printf buffer, clearing the DB_PRINTF flag in 
 * DOORBELL0 and setting it in DOORBELL1.
 * (ODBR and IDBR respectively for the i960Rx adapters)
 */
#define	AAC_DB_PRINTF		(1<<5)

/*
 * Mask containing the interrupt bits we care about.  We don't anticipate
 * (or want) interrupts not in this mask.
 */
#define	AAC_DB_INTERRUPTS \
	(AAC_DB_COMMAND_READY | AAC_DB_RESPONSE_READY | AAC_DB_PRINTF)

/*
 * Queue names
 *
 * Note that we base these at 0 in order to use them as array indices.  Adaptec
 * used base 1 for some unknown reason, and sorted them in a different order.
 */
#define	AAC_HOST_NORM_CMD_QUEUE		0
#define	AAC_HOST_HIGH_CMD_QUEUE		1
#define	AAC_ADAP_NORM_CMD_QUEUE		2
#define	AAC_ADAP_HIGH_CMD_QUEUE		3
#define	AAC_HOST_NORM_RESP_QUEUE	4
#define	AAC_HOST_HIGH_RESP_QUEUE	5
#define	AAC_ADAP_NORM_RESP_QUEUE	6
#define	AAC_ADAP_HIGH_RESP_QUEUE	7

/*
 * List structure used to chain FIBs (used by the adapter - we hang FIBs off
 * our private command structure and don't touch these)
 */
struct aac_fib_list_entry {
	struct fib_list_entry *Flink;
	struct fib_list_entry *Blink;
} __attribute__((__packed__));

/*
 * FIB (FSA Interface Block?); this is the datastructure passed between the
 * host and adapter.
 */
struct aac_fib_header {
	u_int32_t XferState;
	u_int16_t Command;
	u_int8_t StructType;
	u_int8_t Flags;
	u_int16_t Size;
	u_int16_t SenderSize;
	u_int32_t SenderFibAddress;
	u_int32_t ReceiverFibAddress;
	u_int32_t SenderData;
	union {
		struct {
			u_int32_t ReceiverTimeStart;
			u_int32_t ReceiverTimeDone;
		} _s;
		struct aac_fib_list_entry FibLinks;
	} _u;
} __attribute__((__packed__));

#define	AAC_FIB_DATASIZE (512 - sizeof(struct aac_fib_header))

struct aac_fib {
	struct aac_fib_header Header;
	u_int8_t data[AAC_FIB_DATASIZE];
} __attribute__((__packed__));

/*
 * FIB commands
 */
#define	TestCommandResponse		1
#define	TestAdapterCommand		2

/* Lowlevel and comm commands */
#define	LastTestCommand			100
#define	ReinitHostNormCommandQueue	101
#define	ReinitHostHighCommandQueue	102
#define	ReinitHostHighRespQueue		103
#define	ReinitHostNormRespQueue		104
#define	ReinitAdapNormCommandQueue	105
#define	ReinitAdapHighCommandQueue	107
#define	ReinitAdapHighRespQueue		108
#define	ReinitAdapNormRespQueue		109
#define	InterfaceShutdown		110
#define	DmaCommandFib			120
#define	StartProfile			121
#define	TermProfile			122
#define	SpeedTest			123
#define	TakeABreakPt			124
#define	RequestPerfData			125
#define	SetInterruptDefTimer		126
#define	SetInterruptDefCount		127
#define	GetInterruptDefStatus		128
#define	LastCommCommand			129

/* filesystem commands */
#define	NuFileSystem			300
#define	UFS				301
#define	HostFileSystem			302
#define	LastFileSystemCommand		303

/* Container Commands */
#define	ContainerCommand		500
#define	ContainerCommand64		501

/* Cluster Commands */
#define	ClusterCommand			550

/* Scsi Port commands (scsi passthrough) */
#define	ScsiPortCommand			600

/* Misc house keeping and generic adapter initiated commands */
#define	AifRequest			700
#define	CheckRevision			701
#define	FsaHostShutdown			702
#define	RequestAdapterInfo		703
#define	IsAdapterPaused			704
#define	SendHostTime			705
#define	LastMiscCommand			706

/*
 * FIB types
 */
#define	AAC_FIBTYPE_TFIB		1
#define	AAC_FIBTYPE_TQE			2
#define	AAC_FIBTYPE_TCTPERF		3

/*
 * FIB transfer state
 */
#define	AAC_FIBSTATE_HOSTOWNED		(1<<0)	/* owned by the host */
#define	AAC_FIBSTATE_ADAPTEROWNED	(1<<1)	/* owned by the adapter */
#define	AAC_FIBSTATE_INITIALISED	(1<<2)	/* initialised */
#define	AAC_FIBSTATE_EMPTY		(1<<3)	/* empty */
#define	AAC_FIBSTATE_FROMPOOL		(1<<4)	/* allocated from pool */
#define	AAC_FIBSTATE_FROMHOST		(1<<5)	/* sent from the host */
#define	AAC_FIBSTATE_FROMADAP		(1<<6)	/* sent from the adapter */
#define	AAC_FIBSTATE_REXPECTED		(1<<7)	/* response is expected */
#define	AAC_FIBSTATE_RNOTEXPECTED	(1<<8)	/* response is not expected */
#define	AAC_FIBSTATE_DONEADAP		(1<<9)	/* processed by the adapter */
#define	AAC_FIBSTATE_DONEHOST		(1<<10)	/* processed by the host */
#define	AAC_FIBSTATE_HIGH		(1<<11)	/* high priority */
#define	AAC_FIBSTATE_NORM		(1<<12)	/* normal priority */
#define	AAC_FIBSTATE_ASYNC		(1<<13)
#define	AAC_FIBSTATE_ASYNCIO		(1<<13)	/* to be removed */
#define	AAC_FIBSTATE_PAGEFILEIO		(1<<14)	/* to be removed */
#define	AAC_FIBSTATE_SHUTDOWN		(1<<15)
#define	AAC_FIBSTATE_LAZYWRITE		(1<<16)	/* to be removed */
#define	AAC_FIBSTATE_ADAPMICROFIB	(1<<17)
#define	AAC_FIBSTATE_BIOSFIB		(1<<18)
#define	AAC_FIBSTATE_FAST_RESPONSE	(1<<19)	/* fast response capable */
#define	AAC_FIBSTATE_APIFIB		(1<<20)

/*
 * FIB error values
 */
#define	AAC_ERROR_NORMAL			0x00
#define	AAC_ERROR_PENDING			0x01
#define	AAC_ERROR_FATAL				0x02
#define	AAC_ERROR_INVALID_QUEUE			0x03
#define	AAC_ERROR_NOENTRIES			0x04
#define	AAC_ERROR_SENDFAILED			0x05
#define	AAC_ERROR_INVALID_QUEUE_PRIORITY	0x06
#define	AAC_ERROR_FIB_ALLOCATION_FAILED		0x07
#define	AAC_ERROR_FIB_DEALLOCATION_FAILED	0x08

/*
 *  Adapter Status Register
 *
 *  Phase Staus mailbox is 32bits:
 *  <31:16> = Phase Status
 *  <15:0>  = Phase
 *
 *  The adapter reports its present state through the phase.  Only
 *  a single phase should be ever be set.  Each phase can have multiple
 *  phase status bits to provide more detailed information about the
 *  state of the adapter.
 */
#define	AAC_SELF_TEST_FAILED	0x00000004
#define	AAC_UP_AND_RUNNING	0x00000080
#define	AAC_KERNEL_PANIC	0x00000100

#endif	/* !_PCI_AACREG_H_ */

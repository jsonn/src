/*	$NetBSD: atareg.h,v 1.6.6.1 2001/08/24 00:09:04 nathanw Exp $	*/

/*
 * Drive parameter structure for ATA/ATAPI.
 * Bit fields: WDC_* : common to ATA/ATAPI
 *             ATA_* : ATA only
 *             ATAPI_* : ATAPI only.
 */
struct ataparams {
    /* drive info */
    u_int16_t	atap_config;		/* 0: general configuration */
#define WDC_CFG_ATAPI_MASK    	0xc000
#define WDC_CFG_ATAPI    	0x8000
#define	ATA_CFG_REMOVABLE	0x0080
#define	ATA_CFG_FIXED		0x0040
#define ATAPI_CFG_TYPE_MASK	0x1f00
#define ATAPI_CFG_TYPE(x) (((x) & ATAPI_CFG_TYPE_MASK) >> 8)
#define	ATAPI_CFG_REMOV		0x0080
#define ATAPI_CFG_DRQ_MASK	0x0060
#define ATAPI_CFG_STD_DRQ	0x0000
#define ATAPI_CFG_IRQ_DRQ	0x0020
#define ATAPI_CFG_ACCEL_DRQ	0x0040
#define ATAPI_CFG_CMD_MASK	0x0003
#define ATAPI_CFG_CMD_12	0x0000
#define ATAPI_CFG_CMD_16	0x0001
/* words 1-9 are ATA only */
    u_int16_t	atap_cylinders;		/* 1: # of non-removable cylinders */
    u_int16_t	__reserved1;
    u_int16_t	atap_heads;		/* 3: # of heads */
    u_int16_t	__retired1[2];		/* 4-5: # of unform. bytes/track */
    u_int16_t	atap_sectors;		/* 6: # of sectors */
    u_int16_t	__retired2[3];

    u_int8_t	atap_serial[20];	/* 10-19: serial number */
    u_int16_t	__retired3[2];
    u_int16_t	__obsolete1;
    u_int8_t	atap_revision[8];	/* 23-26: firmware revision */
    u_int8_t	atap_model[40];		/* 27-46: model number */
    u_int16_t	atap_multi;		/* 47: maximum sectors per irq (ATA) */
    u_int16_t	__reserved2;
    u_int16_t	atap_capabilities1;	/* 49: capability flags */
#define WDC_CAP_IORDY	0x0800
#define WDC_CAP_IORDY_DSBL 0x0400
#define	WDC_CAP_LBA	0x0200
#define	WDC_CAP_DMA	0x0100
#define ATA_CAP_STBY	0x2000
#define ATAPI_CAP_INTERL_DMA	0x8000
#define ATAPI_CAP_CMD_QUEUE	0x4000
#define	ATAPI_CAP_OVERLP	0X2000
#define ATAPI_CAP_ATA_RST	0x1000
    u_int16_t	atap_capabilities2;	/* 50: capability flags (ATA) */
#if BYTE_ORDER == LITTLE_ENDIAN
    u_int8_t	__junk2;
    u_int8_t	atap_oldpiotiming;	/* 51: old PIO timing mode */
    u_int8_t	__junk3;
    u_int8_t	atap_olddmatiming;	/* 52: old DMA timing mode (ATA) */
#else
    u_int8_t	atap_oldpiotiming;	/* 51: old PIO timing mode */
    u_int8_t	__junk2;
    u_int8_t	atap_olddmatiming;	/* 52: old DMA timing mode (ATA) */
    u_int8_t	__junk3;
#endif
    u_int16_t	atap_extensions;	/* 53: extensions supported */
#define WDC_EXT_UDMA_MODES	0x0004
#define WDC_EXT_MODES		0x0002
#define WDC_EXT_GEOM		0x0001
/* words 54-62 are ATA only */
    u_int16_t	atap_curcylinders;	/* 54: current logical cyliners */
    u_int16_t	atap_curheads;		/* 55: current logical heads */
    u_int16_t	atap_cursectors;	/* 56: current logical sectors/tracks */
    u_int16_t	atap_curcapacity[2];	/* 57-58: current capacity */
    u_int16_t	atap_curmulti;		/* 59: current multi-sector setting */
#define WDC_MULTI_VALID 0x0100
#define WDC_MULTI_MASK  0x00ff
    u_int16_t	atap_capacity[2];  	/* 60-61: total capacity (LBA only) */
    u_int16_t	__retired4;
#if BYTE_ORDER == LITTLE_ENDIAN
    u_int8_t	atap_dmamode_supp; 	/* 63: multiword DMA mode supported */
    u_int8_t	atap_dmamode_act; 	/*     multiword DMA mode active */
    u_int8_t	atap_piomode_supp;       /* 64: PIO mode supported */
    u_int8_t	__junk4;
#else
    u_int8_t	atap_dmamode_act; 	/*     multiword DMA mode active */
    u_int8_t	atap_dmamode_supp; 	/* 63: multiword DMA mode supported */
    u_int8_t	__junk4;
    u_int8_t	atap_piomode_supp;       /* 64: PIO mode supported */
#endif
    u_int16_t	atap_dmatiming_mimi;	/* 65: minimum DMA cycle time */
    u_int16_t	atap_dmatiming_recom;	/* 66: recomended DMA cycle time */
    u_int16_t	atap_piotiming;    	/* 67: mini PIO cycle time without FC */
    u_int16_t	atap_piotiming_iordy;	/* 68: mini PIO cycle time with IORDY FC */
    u_int16_t	__reserved3[2];
/* words 71-72 are ATAPI only */
    u_int16_t	atap_pkt_br;		/* 71: time (ns) to bus release */
    u_int16_t	atap_pkt_bsyclr;	/* 72: tme to clear BSY after service */
    u_int16_t	__reserved4[2];	
    u_int16_t	atap_queuedepth;   	/* 75: */
#define WDC_QUEUE_DEPTH_MASK 0x0F
    u_int16_t	__reserved5[4];   	
    u_int16_t	atap_ata_major;  	/* 80: Major version number */
#define	WDC_VER_ATA1	0x0002
#define	WDC_VER_ATA2	0x0004
#define	WDC_VER_ATA3	0x0008
#define	WDC_VER_ATA4	0x0010
#define	WDC_VER_ATA5	0x0020
    u_int16_t   atap_ata_minor;  	/* 81: Minor version number */
    u_int16_t	atap_cmd_set1;    	/* 82: command set supported */
#define WDC_CMD1_NOP	0x4000
#define WDC_CMD1_RB	0x2000
#define WDC_CMD1_WB	0x1000
#define WDC_CMD1_HPA	0x0400
#define WDC_CMD1_DVRST	0x0200
#define WDC_CMD1_SRV	0x0100
#define WDC_CMD1_RLSE	0x0080
#define WDC_CMD1_AHEAD	0x0040
#define WDC_CMD1_CACHE	0x0020
#define WDC_CMD1_PKT	0x0010
#define WDC_CMD1_PM	0x0008
#define WDC_CMD1_REMOV	0x0004
#define WDC_CMD1_SEC	0x0002
#define WDC_CMD1_SMART	0x0001
    u_int16_t	atap_cmd_set2;    	/* 83: command set supported */
#define WDC_CMD2_RMSN	0x0010
#define WDC_CMD2_DM	0x0001
#define ATA_CMD2_APM	0x0008
#define ATA_CMD2_CFA	0x0004
#define ATA_CMD2_RWQ	0x0002
    u_int16_t	atap_cmd_ext;		/* 84: command/features supp. ext. */
    u_int16_t	atap_cmd1_en;		/* 85: cmd/features enabled */
/* bits are the same as atap_cmd_set1 */
    u_int16_t	atap_cmd2_en;		/* 86: cmd/features enabled */
/* bits are the same as atap_cmd_set2 */
    u_int16_t	atap_cmd_def;		/* 87: cmd/features default */
#if BYTE_ORDER == LITTLE_ENDIAN
    u_int8_t	atap_udmamode_supp; 	/* 88: Ultra-DMA mode supported */
    u_int8_t	atap_udmamode_act; 	/*     Ultra-DMA mode active */
#else
    u_int8_t	atap_udmamode_act; 	/*     Ultra-DMA mode active */
    u_int8_t	atap_udmamode_supp; 	/* 88: Ultra-DMA mode supported */
#endif
/* 89-92 are ATA-only */
    u_int16_t	atap_seu_time;		/* 89: Sec. Erase Unit compl. time */
    u_int16_t	atap_eseu_time;		/* 90: Enhanced SEU compl. time */
    u_int16_t	atap_apm_val;		/* 91: current APM value */
    u_int16_t	__reserved6[35];	/* 92-126: reserved */
    u_int16_t	atap_rmsn_supp;		/* 127: remov. media status notif. */
#define WDC_RMSN_SUPP_MASK 0x0003
#define WDC_RMSN_SUPP 0x0001
    u_int16_t	atap_sec_st;		/* 128: security status */
#define WDC_SEC_LEV_MAX	0x0100
#define WDC_SEC_ESE_SUPP 0x0020
#define WDC_SEC_EXP	0x0010
#define WDC_SEC_FROZEN	0x0008
#define WDC_SEC_LOCKED	0x0004
#define WDC_SEC_EN	0x0002
#define WDC_SEC_SUPP	0x0001
};

/*	$NetBSD: atavar.h,v 1.1.2.5 1998/10/04 15:01:54 bouyer Exp $	*/

/*
 * Copyright (c) 1998 Manuel Bouyer.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 */

/* Hight-level functions and structures used by both ATA and ATAPI devices */

/* Datas common to drives and controller drivers */
struct ata_drive_datas {
    u_int8_t drive; /* drive number */
    u_int8_t drive_flags; /* bitmask for drives present/absent and cap */
#define DRIVE_ATA   0x01
#define DRIVE_ATAPI 0x02
#define DRIVE (DRIVE_ATA|DRIVE_ATAPI)
#define DRIVE_CAP32 0x04
#define DRIVE_DMA   0x08
#define DRIVE_UDMA  0x10
    /*
     * Current setting of drive's PIO, DMA and UDMA modes.
     * Is initialised by the disks drivers at attach time, and may be
     * changed later by the controller's code if needed
     */
    u_int8_t PIO_mode; /* Current setting of drive's PIO mode */
    u_int8_t DMA_mode; /* Current setting of drive's DMA mode */
    u_int8_t UDMA_mode; /* Current setting of drive's UDMA mode */
    /*
     * Drive state. This is drive-type (ATA or ATAPI) dependant
     * This is reset to 0 after a channel reset.
     */
    u_int8_t state;

    struct device *drv_softc; /* ATA drives softc, if any */
    void* chnl_softc; /* channel softc */
};

/* ATA/ATAPI common attachement datas */
struct ata_atapi_attach {
    u_int8_t aa_type; /* Type of device */
#define T_ATA 0
#define T_ATAPI 1
    u_int8_t aa_channel; /* controller's channel */
    u_int8_t aa_openings; /* Number of simultaneous commands possible */
    struct ata_drive_datas *aa_drv_data;
    void *aa_bus_private; /* infos specifics to this bus */
};




/*
 * ATA/ATAPI commands description 
 *
 * This structure defines the interface between the ATA/ATAPI device driver
 * and the controller for short commands. It contains the command's parameter,
 * the len of data's to read/write (if any), and a function to call upon
 * completion.
 * If no sleep is allowed, the driver can poll for command completion.
 * Once the command completed, if the error registed is valid, the flag
 * AT_ERROR is set and the error register value is copied to r_error .
 * A separate interface is needed for read/write or ATAPI packet commands
 * (which need multiple interrupts per commands).
 */
struct wdc_command {
    u_int8_t r_command;  /* Parameters to upload to registers */
    u_int8_t r_head;
    u_int16_t r_cyl;
    u_int8_t r_sector;
    u_int8_t r_count;
    u_int8_t r_precomp;
    u_int8_t r_st_bmask; /* status register mask to wait for before command */
    u_int8_t r_st_pmask; /* status register mask to wait for after command */
    u_int8_t r_error;    /* error register after command done */
    volatile u_int16_t flags;
#define AT_READ     0x0001 /* There is data to read */
#define AT_WRITE    0x0002 /* There is data to write (excl. with AT_READ) */
#define AT_WAIT     0x0008 /* wait in controller code for command completion */
#define AT_POLL     0x0010 /* poll for command completion (no interrupts) */
#define AT_DONE     0x0020 /* command is done */
#define AT_ERROR    0x0040 /* command is done with error */
#define AT_TIMEOU   0x0040 /* command timed out */
#define AT_DF       0x0080 /* Drive fault */
    int timeout;	 /* timeout (in ms) */
    void *data;          /* Data buffer address */
    int bcount;           /* number of bytes to transfer */
    void (*callback) __P((void*)); /* command to call once command completed */
    void *callback_arg;  /* argument passed to *callback() */
};

int wdc_exec_command __P((struct ata_drive_datas *, struct wdc_command*));
#define WDC_COMPLETE 0x01
#define WDC_QUEUED   0x02
#define WDC_TRY_AGAIN 0x03

void wdc_probe_caps __P((struct ata_drive_datas*));

void wdc_reset_channel __P((struct ata_drive_datas *));

struct ataparams;
int ata_get_params __P((struct ata_drive_datas*, u_int8_t,
	 struct ataparams *));
int ata_set_mode __P((struct ata_drive_datas*, u_int8_t, u_int8_t));
/* return code for these cmds */
#define CMD_OK    0
#define CMD_ERR   1
#define CMD_AGAIN 2

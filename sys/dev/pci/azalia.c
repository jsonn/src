/*	$NetBSD: azalia.c,v 1.7.2.2 2005/07/02 16:39:31 tron Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by TAMURA Kent
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: azalia.c,v 1.7.2.2 2005/07/02 16:39:31 tron Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

/* ----------------------------------------------------------------
 * High Definition Audio constant values
 * ---------------------------------------------------------------- */

/* High Definition Audio registers */
#define HDA_GCAP	0x000	/* 2 */
#define		HDA_GCAP_OSS(x)	((x & 0xf000) >> 12)
#define		HDA_GCAP_ISS(x)	((x & 0x0f00) >> 8)
#define		HDA_GCAP_BSS(x)	((x & 0x00f8) >> 3)
#define		HDA_GCAP_NSDO_MASK	0x0006
#define		HDA_GCAP_NSDO_1		0x0000
#define		HDA_GCAP_NSDO_2		0x0002
#define		HDA_GCAP_NSDO_4		0x0004
#define		HDA_GCAP_NSDO_RESERVED	0x0006
#define		HDA_GCAP_64OK	0x0001
#define HDA_VMIN	0x002	/* 1 */
#define HDA_VMAJ	0x003	/* 1 */
#define HDA_OUTPAY	0x004	/* 2 */
#define HDA_INPAY	0x006	/* 2 */
#define HDA_GCTL	0x008	/* 4 */
#define		HDA_GCTL_UNSOL	0x00000080
#define		HDA_GCTL_FCNTRL	0x00000002
#define		HDA_GCTL_CRST	0x00000001
#define HDA_WAKEEN	0x00c	/* 2 */
#define		HDA_WAKEEN_SDIWEN	0x7fff
#define HDA_STATESTS	0x00e	/* 2 */
#define		HDA_STATESTS_SDIWAKE	0x7fff
#define HDA_GSTS	0x010	/* 2 */
#define		HDA_GSTS_FSTS		0x0002
#define HDA_OUTSTRMPAY	0x018	/* 2 */
#define HDA_INSTRMPAY	0x01a	/* 2 */
#define HDA_INTCTL	0x020	/* 4 */
#define		HDA_INTCTL_GIE	0x80000000
#define		HDA_INTCTL_CIE	0x40000000
#define		HDA_INTCTL_SIE	0x3fffffff
#define HDA_INTSTS	0x024	/* 4 */
#define		HDA_INTSTS_GIS	0x80000000
#define		HDA_INTSTS_CIS	0x40000000
#define		HDA_INTSTS_SIS	0x3fffffff
#define HDA_WALCLK	0x030	/* 4 */
#define HDA_SSYNC	0x034	/* 4 */
#define		HDA_SSYNC_SSYNC	0x3fffffff
#define HDA_CORBLBASE	0x040	/* 4 */
#define HDA_CORBUBASE	0x044	/* 4 */
#define HDA_CORBWP	0x048	/* 2 */
#define		HDA_CORBWP_CORBWP	0x00ff
#define HDA_CORBRP	0x04a	/* 2 */
#define		HDA_CORBRP_CORBRPRST	0x8000
#define		HDA_CORBRP_CORBRP	0x00ff
#define HDA_CORBCTL	0x04c	/* 1 */
#define		HDA_CORBCTL_CORBRUN	0x02
#define		HDA_CORBCTL_CMEIE	0x01
#define HDA_CORBSTS	0x04d	/* 1 */
#define		HDA_CORBSTS_CMEI	0x01
#define HDA_CORBSIZE	0x04e	/* 1 */
#define		HDA_CORBSIZE_CORBSZCAP_MASK	0xf0
#define		HDA_CORBSIZE_CORBSZCAP_2	0x10
#define		HDA_CORBSIZE_CORBSZCAP_16	0x20
#define		HDA_CORBSIZE_CORBSZCAP_256	0x40
#define		HDA_CORBSIZE_CORBSIZE_MASK	0x03
#define		HDA_CORBSIZE_CORBSIZE_2		0x00
#define		HDA_CORBSIZE_CORBSIZE_16	0x01
#define		HDA_CORBSIZE_CORBSIZE_256	0x02
#define HDA_RIRBLBASE	0x050	/* 4 */
#define HDA_RIRBUBASE	0x054	/* 4 */
#define HDA_RIRBWP	0x058	/* 2 */
#define		HDA_RIRBWP_RIRBWPRST	0x8000
#define		HDA_RIRBWP_RIRBWP	0x00ff
#define HDA_RINTCNT	0x05a	/* 2 */
#define		HDA_RINTCNT_RINTCNT	0x00ff
#define HDA_RIRBCTL	0x05c	/* 1 */
#define		HDA_RIRBCTL_RIRBOIC	0x04
#define		HDA_RIRBCTL_RIRBDMAEN	0x02
#define		HDA_RIRBCTL_RINTCTL	0x01
#define HDA_RIRBSTS	0x05d	/* 1 */
#define		HDA_RIRBSTS_RIRBOIS	0x04
#define		HDA_RIRBSTS_RINTFL	0x01
#define HDA_RIRBSIZE	0x05e	/* 1 */
#define		HDA_RIRBSIZE_RIRBSZCAP_MASK	0xf0
#define		HDA_RIRBSIZE_RIRBSZCAP_2	0x10
#define		HDA_RIRBSIZE_RIRBSZCAP_16	0x20
#define		HDA_RIRBSIZE_RIRBSZCAP_256	0x40
#define		HDA_RIRBSIZE_RIRBSIZE_MASK	0x03
#define		HDA_RIRBSIZE_RIRBSIZE_2		0x00
#define		HDA_RIRBSIZE_RIRBSIZE_16	0x01
#define		HDA_RIRBSIZE_RIRBSIZE_256	0x02
#define HDA_IC		0x060	/* 4 */
#define HDA_IR		0x064	/* 4 */
#define HDA_IRS		0x068	/* 2 */
#define		HDA_IRS_IRRADD		0x00f0
#define		HDA_IRS_IRRUNSOL	0x0008
#define		HDA_IRS_IRV		0x0002
#define		HDA_IRS_ICB		0x0001
#define HDA_DPLBASE	0x070	/* 4 */
#define		HDA_DPLBASE_DPLBASE	0xffffff80
#define		HDA_DPLBASE_ENABLE	0x00000001
#define HDA_DPUBASE	0x074

#define HDA_SD_BASE	0x080
#define		HDA_SD_CTL	0x00 /* 2 */
#define			HDA_SD_CTL_DEIE	0x0010
#define			HDA_SD_CTL_FEIE	0x0008
#define			HDA_SD_CTL_IOCE	0x0004
#define			HDA_SD_CTL_RUN	0x0002
#define			HDA_SD_CTL_SRST	0x0001
#define		HDA_SD_CTL2	0x02 /* 1 */
#define			HDA_SD_CTL2_STRM	0xf0
#define			HDA_SD_CTL2_STRM_SHIFT	4
#define			HDA_SD_CTL2_DIR		0x08
#define			HDA_SD_CTL2_TP		0x04
#define			HDA_SD_CTL2_STRIPE	0x03
#define		HDA_SD_STS	0x03 /* 1 */
#define			HDA_SD_STS_FIFORDY	0x20
#define			HDA_SD_STS_DESE		0x10
#define			HDA_SD_STS_FIFOE	0x08
#define			HDA_SD_STS_BCIS		0x04
#define		HDA_SD_LPIB	0x04 /* 4 */
#define		HDA_SD_CBL	0x08 /* 4 */
#define		HDA_SD_LVI	0x0c /* 2 */
#define			HDA_SD_LVI_LVI	0x00ff
#define		HDA_SD_FIFOW	0x0e /* 2 */
#define		HDA_SD_FIFOS	0x10 /* 2 */
#define		HDA_SD_FMT	0x12 /* 2 */
#define			HDA_SD_FMT_BASE	0x4000
#define			HDA_SD_FMT_BASE_48	0x0000
#define			HDA_SD_FMT_BASE_44	0x4000
#define			HDA_SD_FMT_MULT	0x3800
#define			HDA_SD_FMT_MULT_X1	0x0000
#define			HDA_SD_FMT_MULT_X2	0x0800
#define			HDA_SD_FMT_MULT_X3	0x1000
#define			HDA_SD_FMT_MULT_X4	0x1800
#define			HDA_SD_FMT_DIV	0x0700
#define			HDA_SD_FMT_DIV_BY1	0x0000
#define			HDA_SD_FMT_DIV_BY2	0x0100
#define			HDA_SD_FMT_DIV_BY3	0x0200
#define			HDA_SD_FMT_DIV_BY4	0x0300
#define			HDA_SD_FMT_DIV_BY5	0x0400
#define			HDA_SD_FMT_DIV_BY6	0x0500
#define			HDA_SD_FMT_DIV_BY7	0x0600
#define			HDA_SD_FMT_DIV_BY8	0x0700
#define			HDA_SD_FMT_BITS	0x0070
#define			HDA_SD_FMT_BITS_8_16	0x0000
#define			HDA_SD_FMT_BITS_16_16	0x0010
#define			HDA_SD_FMT_BITS_20_32	0x0020
#define			HDA_SD_FMT_BITS_24_32	0x0030
#define			HDA_SD_FMT_BITS_32_32	0x0040
#define			HDA_SD_FMT_CHAN	0x000f
#define		HDA_SD_BDPL	0x18 /* 4 */
#define		HDA_SD_BDPU	0x1c /* 4 */
#define		HDA_SD_SIZE	0x20

/* CORB commands */
#define CORB_GET_PARAMETER		0xf00
#define		COP_VENDOR_ID			0x00
#define		COP_REVISION_ID			0x02
#define		COP_SUBORDINATE_NODE_COUNT	0x04
#define			COP_START_NID(x)	((x & 0x00ff0000) >> 16)
#define			COP_NSUBNODES(x)	(x & 0x000000ff)
#define		COP_FUNCTION_GROUP_TYPE		0x05
#define			COP_FTYPE(x)		(x & 0x000000ff)
#define			COP_FTYPE_RESERVED	0x01
#define			COP_FTYPE_AUDIO		0x01
#define			COP_FTYPE_MODEM		0x02
#define		COP_AUDIO_FUNCTION_GROUP_CAPABILITY	0x08
#define		COP_AUDIO_WIDGET_CAPABILITIES	0x09
#define			COP_AWCAP_TYPE(x)	((x >> 20) & 0xf)
#define			COP_AWTYPE_AUDIO_OUTPUT		0x0
#define			COP_AWTYPE_AUDIO_INPUT		0x1
#define			COP_AWTYPE_AUDIO_MIXER		0x2
#define			COP_AWTYPE_AUDIO_SELECTOR	0x3
#define			COP_AWTYPE_PIN_COMPLEX		0x4
#define			COP_AWTYPE_POWER		0x5
#define			COP_AWTYPE_VOLUME_KNOB		0x6
#define			COP_AWTYPE_BEEP_GENERATOR	0x7
#define			COP_AWTYPE_VENDOR_DEFINED	0xf
#define			COP_AWCAP_STEREO	0x001
#define			COP_AWCAP_INAMP		0x002
#define			COP_AWCAP_OUTAMP	0x004
#define			COP_AWCAP_AMPOV		0x008
#define			COP_AWCAP_FORMATOV	0x010
#define			COP_AWCAP_STRIPE	0x020
#define			COP_AWCAP_PROC		0x040
#define			COP_AWCAP_UNSOL		0x080
#define			COP_AWCAP_CONLIST	0x100
#define			COP_AWCAP_DIGITAL	0x200
#define			COP_AWCAP_POWER		0x400
#define			COP_AWCAP_LRSWAP	0x800
#define			COP_AWCAP_DELAY(x)	((x >> 16) & 0xf)
#define		COP_PCM				0x0a
#define			COP_PCM_B32	0x00100000
#define			COP_PCM_B24	0x00080000
#define			COP_PCM_B20	0x00040000
#define			COP_PCM_B16	0x00020000
#define			COP_PCM_B8	0x00010000
#define			COP_PCM_R3840	0x00000800
#define			COP_PCM_R1920	0x00000400
#define			COP_PCM_R1764	0x00000200
#define			COP_PCM_R960	0x00000100
#define			COP_PCM_R882	0x00000080
#define			COP_PCM_R480	0x00000040
#define			COP_PCM_R441	0x00000020
#define			COP_PCM_R320	0x00000010
#define			COP_PCM_R220	0x00000008
#define			COP_PCM_R160	0x00000004
#define			COP_PCM_R110	0x00000002
#define			COP_PCM_R80	0x00000001
#define		COP_STREAM_FORMATS		0x0b
#define			COP_STREAM_FORMAT_PCM		0x00000001
#define			COP_STREAM_FORMAT_FLOAT32	0x00000002
#define			COP_STREAM_FORMAT_AC3		0x00000003
#define		COP_PIN_CAPABILITIES		0x0c
#define			COP_PINCAP_IMPEDANCE	0x00000001
#define			COP_PINCAP_TRIGGER	0x00000002
#define			COP_PINCAP_PRESENCE	0x00000004
#define			COP_PINCAP_HEADPHONE	0x00000008
#define			COP_PINCAP_OUTPUT	0x00000010
#define			COP_PINCAP_INPUT	0x00000020
#define			COP_PINCAP_BALANCE	0x00000040
#define			COP_PINCAP_VREF(x)	((x >> 8) & 0xff)
#define			COP_PINCAP_EAPD		0x00010000
#define		COP_INPUT_AMPLIFIER_CAPABILITIES	0x0d
#define		COP_CONNECTION_LIST_LENGTH	0x0e
#define			COP_CLL_LONG		0x00000080
#define			COP_CLL_LENGTH(x)	(x & 0x0000007f)
#define		COP_SUPPORTED_POWER_STATES	0x0f
#define		COP_PROCESSING_CAPABILITIES	0x10
#define		COP_GPIO_COUNT			0x11
#define		COP_OUTPUT_AMPLIFIER_CAPABILITIES	0x12
#define		COP_VOLUME_KNOB_CAPABILITIES	0x13
#define CORB_GET_CONNECTION_SELECT_CONTROL	0xf01
#define		CORB_CSC_INDEX(x)		(x & 0xff)
#define CORB_SET_CONNECTION_SELECT_CONTROL	0x701
#define CORB_GET_CONNECTION_LIST_ENTRY	0xf02
#define		CORB_CLE_LONG_0(x)	(x & 0x0000ffff)
#define		CORB_CLE_LONG_1(x)	((x & 0xffff0000) >> 16)
#define		CORB_CLE_SHORT_0(x)	(x & 0xff)
#define		CORB_CLE_SHORT_1(x)	((x >> 8) & 0xff)
#define		CORB_CLE_SHORT_2(x)	((x >> 16) & 0xff)
#define		CORB_CLE_SHORT_3(x)	((x >> 24) & 0xff)
#define CORB_GET_PROCESSING_STATE	0xf03
#define CORB_SET_PROCESSING_STATE	0x703
#define CORB_GET_COEFFICIENT_INDEX	0xd00
#define CORB_SET_COEFFICIENT_INDEX	0x500
#define CORB_GET_PROCESSING_COEFFICIENT	0xc00
#define CORB_SET_PROCESSING_COEFFICIENT	0x400
#define CORB_GET_AMPLIFIER_GAIN_MUTE	0xb00
#define CORB_SET_AMPLIFIER_GAIN_MUTE	0x300
#define		CORB_AGM_GAIN_MASK	0x007f
#define		CORB_AGM_MUTE		0x0080
#define		CORB_AGM_INDEX_SHIFT	8
#define		CORB_AGM_RIGHT		0x1000
#define		CORB_AGM_LEFT		0x2000
#define		CORB_AGM_INPUT		0x4000
#define		CORB_AGM_OUTPUT		0x8000
#define CORB_GET_CONVERTER_FORMAT	0xa00
#define CORB_SET_CONVERTER_FORMAT	0x200
#define CORB_GET_DIGITAL_CONVERTER_CONTROL	0xf0d
#define CORB_SET_DIGITAL_CONVERTER_CONTROL_L	0x70d
#define CORB_SET_DIGITAL_CONVERTER_CONTROL_H	0x70e
#define CORB_GET_POWER_STATE		0xf05
#define CORB_SET_POWER_STATE		0x705
#define CORB_GET_CONVERTER_STREAM_CHANNEL	0xf06
#define CORB_SET_CONVERTER_STREAM_CHANNEL	0x706
#define CORB_GET_INPUT_CONVERTER_SDI_SELECT	0xf04
#define CORB_SET_INPUT_CONVERTER_SDI_SELECT	0x704
#define CORB_GET_PIN_WIDGET_CONTROL	0xf07
#define CORB_SET_PIN_WIDGET_CONTROL	0x707
#define		CORB_PWC_HEADPHONE	0x80
#define		CORB_PWC_OUTPUT		0x40
#define		CORB_PWC_INPUT		0x20
#define		CORB_PWC_VREF_HIZ	0x00
#define		CORB_PWC_VREF_50	0x01
#define		CORB_PWC_VREF_GND	0x02
#define		CORB_PWC_VREF_80	0x04
#define		CORB_PWC_VREF_100	0x05
#define CORB_GET_UNSOLICITED_RESPONSE	0xf08
#define CORB_SET_UNSOLICITED_RESPONSE	0x708
#define CORB_GET_PIN_SENSE		0xf09
#define CORB_EXECUTE_PIN_SENSE		0x709
#define CORB_GET_EAPD_BTL_ENABLE	0xf0c
#define CORB_SET_EAPD_BTL_ENABLE	0x70c
#define CORB_GET_GPI_DATA		0xf10
#define CORB_SET_GPI_DATA		0x710
#define CORB_GET_GPI_WAKE_ENABLE_MASK	0xf11
#define CORB_SET_GPI_WAKE_ENABLE_MASK	0x711
#define CORB_GET_GPI_UNSOLICITED_ENABLE_MASK	0xf12
#define CORB_SET_GPI_UNSOLICITED_ENABLE_MASK	0x712
#define CORB_GET_GPI_STICKY_MASK	0xf13
#define CORB_SET_GPI_STICKY_MASK	0x713
#define CORB_GET_GPO_DATA		0xf14
#define CORB_SET_GPO_DATA		0x714
#define CORB_GET_GPIO_DATA		0xf15
#define CORB_SET_GPIO_DATA		0x715
#define CORB_GET_GPIO_ENABLE_MASK	0xf16
#define CORB_SET_GPIO_ENABLE_MASK	0x716
#define CORB_GET_GPIO_DIRECTION		0xf17
#define CORB_SET_GPIO_DIRECTION		0x717
#define CORB_GET_GPIO_WAKE_ENABLE_MASK	0xf18
#define CORB_SET_GPIO_WAKE_ENABLE_MASK	0x718
#define CORB_GET_GPIO_UNSOLICITED_ENABLE_MASK	0xf19
#define CORB_SET_GPIO_UNSOLICITED_ENABLE_MASK	0x719
#define CORB_GET_GPIO_STICKY_MASK	0xf1a
#define CORB_SET_GPIO_STICKY_MASK	0x71a
#define CORB_GET_BEEP_GENERATION	0xf0a
#define CORB_SET_BEEP_GENERATION	0x70a
#define CORB_GET_VOLUME_KNOB		0xf0f
#define CORB_SET_VOLUME_KNOB		0x70f
#define CORB_GET_SUBSYSTEM_ID		0xf20
#define CORB_SET_SUBSYSTEM_ID_1		0x720
#define CORB_SET_SUBSYSTEM_ID_2		0x721
#define CORB_SET_SUBSYSTEM_ID_3		0x722
#define CORB_SET_SUBSYSTEM_ID_4		0x723
#define CORB_GET_CONFIGURATION_DEFAULT	0xf1c
#define CORB_SET_CONFIGURATION_DEFAULT_1	0x71c
#define CORB_SET_CONFIGURATION_DEFAULT_2	0x71d
#define CORB_SET_CONFIGURATION_DEFAULT_3	0x71e
#define CORB_SET_CONFIGURATION_DEFAULT_4	0x71f
#define		CORB_CD_SEQUENCE(x)	(x & 0x0000000f)
#define		CORB_CD_SEQUENCE_MAX	0x0f
#define		CORB_CD_ASSOCIATION(x)	((x >> 4) & 0xf)
#define		CORB_CD_ASSOCIATION_MAX	0x0f
#define		CORB_CD_MISC_MASK	0x00000f00
#define		CORB_CD_COLOR(x)	((x >> 12) & 0xf)
#define			CORB_CD_COLOR_UNKNOWN	0x0
#define			CORB_CD_BLACK	0x1
#define			CORB_CD_GRAY	0x2
#define			CORB_CD_BLUE	0x3
#define			CORB_CD_GREEN	0x4
#define			CORB_CD_RED	0x5
#define			CORB_CD_ORANGE	0x6
#define			CORB_CD_YELLOW	0x7
#define			CORB_CD_PURPLE	0x8
#define			CORB_CD_PINK	0x9
#define			CORB_CD_WHITE	0xe
#define			CORB_CD_COLOR_OTHER	0xf
#define		CORB_CD_CONNECTION_MASK	0x000f0000
#define		CORB_CD_DEVICE(x)	((x >> 20) & 0xf)
#define			CORB_CD_LINEOUT		0x0
#define			CORB_CD_SPEAKER		0x1
#define			CORB_CD_HEADPHONE	0x2
#define			CORB_CD_CD		0x3
#define			CORB_CD_SPDIFOUT	0x4
#define			CORB_CD_DIGITALOUT	0x5
#define			CORB_CD_MODEMLINE	0x6
#define			CORB_CD_MODEMHANDSET	0x7
#define			CORB_CD_LINEIN		0x8
#define			CORB_CD_AUX		0x9
#define			CORB_CD_MICIN		0xa
#define			CORB_CD_TELEPHONY	0xb
#define			CORB_CD_SPDIFIN		0xc
#define			CORB_CD_DIGITALIN	0xd
#define			CORB_CD_DEVICE_OTHER	0xf
#define		CORB_CD_LOCATION_MASK	0x3f000000
#define		CORB_CD_PORT_MASK	0xc0000000
#define CORB_GET_STRIPE_CONTROL		0xf24
#define CORB_SET_STRIPE_CONTROL		0x720	/* XXX typo in the spec? */
#define CORB_EXECUTE_FUNCTION_RESET	0x7ff

#define CORB_NID_ROOT		0
#define HDA_MAX_CHANNELS	16


#define PCI_SUBCLASS_HDAUDIO	0x03

/* memory-mapped types */
typedef struct {
	uint32_t low;
	uint32_t high;
	uint32_t length;
	uint32_t flags;
#define	BDLIST_ENTRY_IOC	0x00000001
} __packed bdlist_entry_t;
#define HDA_BDL_MAX	256

typedef struct {
	uint32_t position;
	uint32_t reserved;
} __packed dmaposition_t;

typedef uint32_t corb_entry_t;
typedef struct {
	uint32_t resp;
	uint32_t resp_ex;
} __packed rirb_entry_t;


/* ----------------------------------------------------------------
 * ICH6/ICH7 constant values
 * ---------------------------------------------------------------- */

/* PCI registers */
#define ICH_PCI_HDBARL	0x10
#define ICH_PCI_HDBARU	0x14
#define ICH_PCI_HDCTL	0x40
#define		ICH_PCI_HDCTL_CLKDETCLR		0x08
#define		ICH_PCI_HDCTL_CLKDETEN		0x04
#define		ICH_PCI_HDCTL_CLKDETINV		0x02
#define		ICH_PCI_HDCTL_SIGNALMODE	0x01

/* #define AZALIA_DEBUG */
#ifdef AZALIA_DEBUG
# define DPRINTF(x)	do { printf x; } while (0/*CONSTCOND*/)
#else
# define DPRINTF(x)	do {} while (0/*CONSTCOND*/)
#endif
#define PTR_UPPER32(x)	((uint64_t)(uintptr_t)x >> 32)


/* internal types */

typedef struct {
	bus_dmamap_t map;
	caddr_t addr;		/* kernel virtual address */
	bus_dma_segment_t segments[1];
	size_t size;
} azalia_dma_t;
#define AZALIA_DMA_DMAADDR(p)	((p)->map->dm_segs[0].ds_addr)

typedef struct {
	int regbase;
	int number;
	uint32_t intr_bit;
	azalia_dma_t bdlist;
	azalia_dma_t buffer;
	void (*intr)(void*);
	void *intr_arg;
	bus_addr_t dmaend, dmanext; /* XXX needed? */
} stream_t;
#define STR_READ_1(z, s, r)	\
	bus_space_read_1((z)->iot, (z)->ioh, (s)->regbase + HDA_SD_##r)
#define STR_READ_2(z, s, r)	\
	bus_space_read_2((z)->iot, (z)->ioh, (s)->regbase + HDA_SD_##r)
#define STR_READ_4(z, s, r)	\
	bus_space_read_4((z)->iot, (z)->ioh, (s)->regbase + HDA_SD_##r)
#define STR_WRITE_1(z, s, r, v)	\
	bus_space_write_1((z)->iot, (z)->ioh, (s)->regbase + HDA_SD_##r, v)
#define STR_WRITE_2(z, s, r, v)	\
	bus_space_write_2((z)->iot, (z)->ioh, (s)->regbase + HDA_SD_##r, v)
#define STR_WRITE_4(z, s, r, v)	\
	bus_space_write_4((z)->iot, (z)->ioh, (s)->regbase + HDA_SD_##r, v)

typedef int nid_t;

typedef struct {
	nid_t nid;
	uint32_t widgetcap;
	int type;		/* = bit20-24 of widgetcap */
	int nconnections;
	nid_t *connections;
	int selected;
	union {
		struct {	/* for AUDIO_INPUT/OUTPUT */
			uint32_t encodings;
			uint32_t bits_rates;
		} audio;
		struct {
		} mixer;
		struct {
		} selector;
		struct {	/* for PIN */
			uint32_t cap;
			uint32_t config;
			int sequence;
			int association;
			int color;
			int device;
		} pin;
		struct {
		} power;
		struct {
		} volume;
		struct {
		} beep;
	} d;
} widget_t;

typedef struct codec_t {
	int (*comresp)(const struct codec_t *, nid_t, uint32_t, uint32_t, uint32_t *);
	struct azalia_t *az;
	int address;
	int nfunctions;
	nid_t audiofunc;	/* NID of an audio function node */
	int nwidgets;		/* # of widgets in the audio function */
	widget_t *widgets;	/* widgets in the audio function */
	int ndacindexes;
	int dacindexes[HDA_MAX_CHANNELS];

	struct audio_format* formats;
	int nformats;
	struct audio_encoding_set *encodings;
} codec_t;

typedef struct azalia_t {
	struct device dev;
	struct device *audiodev;

	void *ih;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_dma_tag_t dmat;

	codec_t codecs[15];
	int ncodecs;

	azalia_dma_t corb_dma;
	int corb_size;
	azalia_dma_t rirb_dma;
	int rirb_size;
	int rirb_rp;

	int nistreams, nostreams, nbstreams;
	stream_t pstream;
	stream_t rstream;
} azalia_t;
#define XNAME(sc)		((sc)->dev.dv_xname)
#define AZ_READ_1(z, r)		bus_space_read_1((z)->iot, (z)->ioh, HDA_##r)
#define AZ_READ_2(z, r)		bus_space_read_2((z)->iot, (z)->ioh, HDA_##r)
#define AZ_READ_4(z, r)		bus_space_read_4((z)->iot, (z)->ioh, HDA_##r)
#define AZ_WRITE_1(z, r, v)	bus_space_write_1((z)->iot, (z)->ioh, HDA_##r, v)
#define AZ_WRITE_2(z, r, v)	bus_space_write_2((z)->iot, (z)->ioh, HDA_##r, v)
#define AZ_WRITE_4(z, r, v)	bus_space_write_4((z)->iot, (z)->ioh, HDA_##r, v)


/* prototypes */
static int	azalia_pci_match(struct device *, struct cfdata *, void *);
static void	azalia_pci_attach(struct device *, struct device *, void *);
static int	azalia_pci_detach(struct device *, int);
static int	azalia_intr(void *);
static int	azalia_attach(azalia_t *);
static void	azalia_attach_intr(struct device *);
static int	azalia_init_corb(azalia_t *);
static int	azalia_init_rirb(azalia_t *);
static int	azalia_set_command(const azalia_t *, nid_t, int, uint32_t, uint32_t);
static int	azalia_get_response(azalia_t *, uint32_t *);
static int	azalia_alloc_dmamem(azalia_t *, size_t, size_t, azalia_dma_t *);
static int	azalia_free_dmamem(const azalia_t *, azalia_dma_t*);

static int	azalia_codec_init(codec_t *);
static void	azalia_codec_add_format(codec_t *, int, int, int, uint32_t);
static int	azalia_codec_find_pin(const codec_t *, uint32_t, int, int);
static int	azalia_codec_find_dac(const codec_t *, int, int);
static int	azalia_codec_nid2index(const codec_t *, nid_t);
static int	azalia_codec_comresp(const codec_t *, nid_t, uint32_t,
	uint32_t, uint32_t *);
static int	azalia_codec_connect_stream(codec_t *, int, uint16_t, int);

static int	azalia_widget_init(widget_t *, const codec_t *, int);
static int	azalia_widget_init_audio(widget_t *, const codec_t *);
static int	azalia_widget_print_audio(const widget_t *, const char *);
static int	azalia_widget_init_pin(widget_t *, const codec_t *);
static int	azalia_widget_init_connection(widget_t *, const codec_t *);
static int	azalia_widget_nid2clindex(const widget_t *, nid_t);
int	azalia_widget_set_connection(const widget_t *, const codec_t *, nid_t);

static int	azalia_query_encoding(void *, audio_encoding_t *);
static int	azalia_set_params(void *, int, int, audio_params_t *,
	audio_params_t *, stream_filter_list_t *, stream_filter_list_t *);
static int	azalia_round_blocksize(void *, int, int, const audio_params_t *);
static int	azalia_halt_output(void *);
static int	azalia_halt_input(void *);
static int	azalia_getdev(void *, struct audio_device *);
static int	azalia_set_port(void *, mixer_ctrl_t *);
static int	azalia_get_port(void *, mixer_ctrl_t *);
static int	azalia_query_devinfo(void *, mixer_devinfo_t *);
static void	*azalia_allocm(void *, int, size_t, struct malloc_type *, int);
static void	azalia_freem(void *, void *, struct malloc_type *);
static size_t	azalia_round_buffersize(void *, int, size_t);
static int	azalia_get_props(void *);
static int	azalia_trigger_output(void *, void *, void *, int,
	void (*)(void *), void *, const audio_params_t *);
static int	azalia_trigger_input(void *, void *, void *, int,
	void (*)(void *), void *, const audio_params_t *);

static int	azalia_params2fmt(const audio_params_t *, uint16_t *);

/* variables */
CFATTACH_DECL(azalia, sizeof(azalia_t),
    azalia_pci_match, azalia_pci_attach, azalia_pci_detach, NULL);

static const struct audio_hw_if azalia_hw_if = {
	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* drain */
	azalia_query_encoding,
	azalia_set_params,
	azalia_round_blocksize,
	NULL,			/* commit_settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* satart_inpu */
	azalia_halt_output,
	azalia_halt_input,
	NULL,			/* speaker_ctl */
	azalia_getdev,
	NULL,			/* setfd */
	azalia_set_port,
	azalia_get_port,
	azalia_query_devinfo,
	azalia_allocm,
	azalia_freem,
	azalia_round_buffersize,
	NULL,			/* mappage */
	azalia_get_props,
	azalia_trigger_output,
	azalia_trigger_input,
	NULL,			/* dev_ioctl */
};

/* ================================================================
 * PCI functions
 * ================================================================ */

static int
azalia_pci_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa;

	pa = aux;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MULTIMEDIA
	    && PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_HDAUDIO)
		return 1;
	return 0;
}

static void
azalia_pci_attach(struct device *parent, struct device *self, void *aux)
{
	azalia_t *sc;
	struct pci_attach_args *pa;
	bus_size_t regsize;
	pcireg_t v;
	pci_intr_handle_t ih;
	const char *intrrupt_str;

	sc = (azalia_t*)self;
	pa = aux;

	sc->dmat = pa->pa_dmat;
	aprint_normal(": Generic High Definition Audio Controller\n");
	if (pci_mapreg_map(pa, ICH_PCI_HDBARL, PCI_MAPREG_MEM_TYPE_64BIT, 0,
			   &sc->iot, &sc->ioh, NULL, &regsize)) {
		aprint_error("%s: can't map device i/o space\n", XNAME(sc));
		return;
	}

	/* enable bus mastering */
	v = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    v | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_BACKTOBACK_ENABLE);

	/* interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error("%s: can't map interrupt\n", XNAME(sc));
		return;
	}
	intrrupt_str = pci_intr_string(pa->pa_pc, ih);
	sc->ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, azalia_intr, sc);
	if (sc->ih == NULL) {
		aprint_error("%s: can't establish interrupt", XNAME(sc));
		if (intrrupt_str != NULL)
			aprint_error(" at %s", intrrupt_str);
		aprint_error("\n");
		return;
	}
	aprint_normal("%s: interrupting at %s\n", XNAME(sc), intrrupt_str);

	if (azalia_attach(sc)) {
		aprint_error("%s: initialization failure\n", XNAME(sc));
		return;
	}
	config_interrupts(self, azalia_attach_intr);
}

static int
azalia_pci_detach(struct device *self, int flags)
{
	azalia_t *az;

	az = (azalia_t*)self;
	azalia_free_dmamem(az, &az->corb_dma);
	return 0;
}

static int
azalia_intr(void *v)
{
	azalia_t *az;
	stream_t *str;
	int ret;
	uint32_t intsts;
	uint8_t rirbsts;

	az = v;
	ret = 0;
	//printf("[i]");

	intsts = AZ_READ_4(az, INTSTS);
	if (intsts == 0)
		return ret;

	str = &az->pstream;
	if (intsts & str->intr_bit) {
		STR_WRITE_1(az, str, STS,
		    HDA_SD_STS_DESE | HDA_SD_STS_FIFOE | HDA_SD_STS_BCIS);
		//printf("[p]");
		str->intr(str->intr_arg);
		ret++;
	}

	rirbsts = AZ_READ_1(az, RIRBSTS);
	if (rirbsts & (HDA_RIRBSTS_RIRBOIS | HDA_RIRBSTS_RINTFL)) {
		if (rirbsts & HDA_RIRBSTS_RINTFL) {
			//printf("[R]");
		} else {
			//printf("[O]");
		}
		AZ_WRITE_1(az, RIRBSTS,
		    rirbsts | HDA_RIRBSTS_RIRBOIS | HDA_RIRBSTS_RINTFL);
		ret++;
	}
	return 0;
}

/* ================================================================
 * HDA controller functions
 * ================================================================ */

static int
azalia_attach(azalia_t *az)
{
	int i, n;
	uint32_t gctl;
	uint16_t gcap;
	uint16_t statests;

	aprint_normal("%s: High Definition Audio rev. %d.%d\n",
	    XNAME(az), AZ_READ_1(az, VMAJ), AZ_READ_1(az, VMIN));
	gcap = AZ_READ_2(az, GCAP);
	az->nistreams = HDA_GCAP_ISS(gcap);
	az->nostreams = HDA_GCAP_OSS(gcap);
	az->nbstreams = HDA_GCAP_BSS(gcap);
	DPRINTF(("%s: %d output, %d input, and %d bidi streams\n",
	    XNAME(az), az->nostreams, az->nistreams, az->nbstreams));

	/* 4.2.2 Starting the High Definition Audio Controller */
	DPRINTF(("%s: resetting\n", __func__));
	gctl = AZ_READ_4(az, GCTL);
	AZ_WRITE_4(az, GCTL, gctl & ~HDA_GCTL_CRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		if ((AZ_READ_4(az, GCTL) & HDA_GCTL_CRST) == 0)
			break;
	}
	DPRINTF(("%s: reset counter = %d\n", __func__, i));
	if (i <= 0) {
		aprint_error("%s: reset failure\n", XNAME(az));
		return ETIMEDOUT;
	}
	DELAY(1000);
	gctl = AZ_READ_4(az, GCTL);
	AZ_WRITE_4(az, GCTL, gctl | HDA_GCTL_CRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		if (AZ_READ_4(az, GCTL) & HDA_GCTL_CRST)
			break;
	}
	DPRINTF(("%s: reset counter = %d\n", __func__, i));
	if (i <= 0) {
		aprint_error("%s: reset-exit failure\n", XNAME(az));
		return ETIMEDOUT;
	}

	/* 4.3 Codec discovery */
	DELAY(1000);
	statests = AZ_READ_2(az, STATESTS);
	for (i = 0, n = 0; i < 15; i++) {
		if ((statests >> i) & 1) {
			DPRINTF(("%s: found a codec at #%d\n", XNAME(az), i));
			az->codecs[n].address = i;
			az->codecs[n++].az = az;
		}
	}
	az->ncodecs = n;
	if (az->ncodecs < 1) {
		aprint_error("%s: No HD-Audio codecs\n", XNAME(az));
		return -1;
	}
	return 0;
}

static void
azalia_attach_intr(struct device *self)
{
	azalia_t *az;
	int err, i;

	az = (azalia_t*)self;

	AZ_WRITE_2(az, STATESTS, HDA_STATESTS_SDIWAKE);
	AZ_WRITE_1(az, RIRBSTS, HDA_RIRBSTS_RINTFL | HDA_RIRBSTS_RIRBOIS);
	AZ_WRITE_4(az, INTSTS, HDA_INTSTS_CIS | HDA_INTSTS_GIS);
	AZ_WRITE_4(az, DPLBASE, 0);
	AZ_WRITE_4(az, DPUBASE, 0);

	/* 4.4.1 Command Outbound Ring Buffer */
	azalia_init_corb(az);
	/* 4.4.2 Response Inbound Ring Buffer */
	azalia_init_rirb(az);

	AZ_WRITE_4(az, INTCTL,
	    AZ_READ_4(az, INTCTL) | HDA_INTCTL_CIE | HDA_INTCTL_GIE);

	for (i = 0; i < az->ncodecs; i++) {
		err = azalia_codec_init(&az->codecs[i]);
		if (err)
			return;
	}

	az->pstream.regbase = HDA_SD_BASE + (az->nistreams + 0) * HDA_SD_SIZE;
	az->pstream.intr_bit = 1 << ((az->pstream.regbase - HDA_SD_BASE) / HDA_SD_SIZE);
	az->pstream.number = 1;
	az->rstream.regbase = HDA_SD_BASE + 0 * HDA_SD_SIZE;
	az->rstream.intr_bit = 1 << ((az->rstream.regbase - HDA_SD_BASE) / HDA_SD_SIZE);
	az->rstream.number = 2;
	/* setup BDL buffers */
	err = azalia_alloc_dmamem(az, sizeof(bdlist_entry_t) * HDA_BDL_MAX,
				  128, &az->pstream.bdlist);
	if (err) {
		aprint_error("%s: can't allocate a BDL buffer\n", XNAME(az));
		return;
	}
	err = azalia_alloc_dmamem(az, sizeof(bdlist_entry_t) * HDA_BDL_MAX,
				  128, &az->rstream.bdlist);
	if (err) {
		aprint_error("%s: can't allocate a BDL buffer\n", XNAME(az));
		return;
	}

	az->audiodev = audio_attach_mi(&azalia_hw_if, az, &az->dev);
	return;
	/* XXX deallocation on errors */
}

static int
azalia_init_corb(azalia_t *az)
{
	int entries, err, i;
	uint16_t corbrp, corbwp;
	uint8_t corbsize, cap, corbctl;

	/* stop the CORB */
	corbctl = AZ_READ_1(az, CORBCTL);
	if (corbctl & HDA_CORBCTL_CORBRUN) { /* running? */
		AZ_WRITE_1(az, CORBCTL, corbctl & ~HDA_CORBCTL_CORBRUN);
		for (i = 5000; i >= 0; i--) {
			DELAY(10);
			corbctl = AZ_READ_1(az, CORBCTL);
			if ((corbctl & HDA_CORBCTL_CORBRUN) == 0)
				break;
		}
		if (i <= 0) {
			aprint_error("%s: CORB is running\n", XNAME(az));
			return EBUSY;
		}
	}

	/* determine CORB size */
	corbsize = AZ_READ_1(az, CORBSIZE);
	cap = corbsize & HDA_CORBSIZE_CORBSZCAP_MASK;
	corbsize &= ~HDA_CORBSIZE_CORBSIZE_MASK;
	if (cap & HDA_CORBSIZE_CORBSZCAP_256) {
		entries = 256;
		corbsize |= HDA_CORBSIZE_CORBSIZE_256;
	} else if (cap & HDA_CORBSIZE_CORBSZCAP_16) {
		entries = 16;
		corbsize |= HDA_CORBSIZE_CORBSIZE_16;
	} else if (cap & HDA_CORBSIZE_CORBSZCAP_2) {
		entries = 2;
		corbsize |= HDA_CORBSIZE_CORBSIZE_2;
	} else {
		aprint_error("%s: Invalid CORBSZCAP: 0x%2x\n", XNAME(az), cap);
		return -1;
	}

	err = azalia_alloc_dmamem(az, entries * sizeof(corb_entry_t),
	    128, &az->corb_dma);
	if (err) {
		aprint_error("%s: can't allocate CORB buffer\n", XNAME(az));
		return err;
	}
	AZ_WRITE_4(az, CORBLBASE, (uint32_t)AZALIA_DMA_DMAADDR(&az->corb_dma));
	AZ_WRITE_4(az, CORBUBASE, PTR_UPPER32(AZALIA_DMA_DMAADDR(&az->corb_dma)));
	AZ_WRITE_1(az, CORBSIZE, corbsize);
	az->corb_size = entries;

	DPRINTF(("%s: CORB allocation succeeded.\n", __func__));

	/* reset CORBRP */
	corbrp = AZ_READ_2(az, CORBRP);
	AZ_WRITE_2(az, CORBRP, corbrp | HDA_CORBRP_CORBRPRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		corbrp = AZ_READ_2(az, CORBRP);
		if (corbrp & HDA_CORBRP_CORBRPRST)
			break;
	}
	if (i <= 0) {
		aprint_error("%s: CORBRP reset failure\n", XNAME(az));
		return -1;
	}
	AZ_WRITE_2(az, CORBRP, corbrp & ~HDA_CORBRP_CORBRPRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		corbrp = AZ_READ_2(az, CORBRP);
		if ((corbrp & HDA_CORBRP_CORBRPRST) == 0)
			break;
	}
	if (i <= 0) {
		aprint_error("%s: CORBRP reset failure 2\n", XNAME(az));
		return -1;
	}
	DPRINTF(("%s: CORBWP=%d; size=%d\n", __func__,
		 AZ_READ_2(az, CORBRP) & HDA_CORBRP_CORBRP, az->corb_size));

	/* clear CORBWP */
	corbwp = AZ_READ_2(az, CORBWP);
	AZ_WRITE_2(az, CORBWP, corbwp & ~HDA_CORBWP_CORBWP);

	/* Run! */
	corbctl = AZ_READ_1(az, CORBCTL);
	AZ_WRITE_1(az, CORBCTL, corbctl | HDA_CORBCTL_CORBRUN);
	return 0;
}

static int
azalia_init_rirb(azalia_t *az)
{
	int entries, err, i;
	uint16_t rirbwp;
	uint8_t rirbsize, cap, rirbctl;

	/* stop the RIRB */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	if (rirbctl & HDA_RIRBCTL_RIRBDMAEN) { /* running? */
		AZ_WRITE_1(az, RIRBCTL, rirbctl & ~HDA_RIRBCTL_RIRBDMAEN);
		for (i = 5000; i >= 0; i--) {
			DELAY(10);
			rirbctl = AZ_READ_1(az, RIRBCTL);
			if ((rirbctl & HDA_RIRBCTL_RIRBDMAEN) == 0)
				break;
		}
		if (i <= 0) {
			aprint_error("%s: RIRB is running\n", XNAME(az));
			return EBUSY;
		}
	}

	/* determine RIRB size */
	rirbsize = AZ_READ_1(az, RIRBSIZE);
	cap = rirbsize & HDA_RIRBSIZE_RIRBSZCAP_MASK;
	rirbsize &= ~HDA_RIRBSIZE_RIRBSIZE_MASK;
	if (cap & HDA_RIRBSIZE_RIRBSZCAP_256) {
		entries = 256;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_256;
	} else if (cap & HDA_RIRBSIZE_RIRBSZCAP_16) {
		entries = 16;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_16;
	} else if (cap & HDA_RIRBSIZE_RIRBSZCAP_2) {
		entries = 2;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_2;
	} else {
		aprint_error("%s: Invalid RIRBSZCAP: 0x%2x\n", XNAME(az), cap);
		return -1;
	}

	err = azalia_alloc_dmamem(az, entries * sizeof(rirb_entry_t),
	    128, &az->rirb_dma);
	if (err) {
		aprint_error("%s: can't allocate RIRB buffer\n", XNAME(az));
		return err;
	}
	AZ_WRITE_4(az, RIRBLBASE, (uint32_t)AZALIA_DMA_DMAADDR(&az->rirb_dma));
	AZ_WRITE_4(az, RIRBUBASE, PTR_UPPER32(AZALIA_DMA_DMAADDR(&az->rirb_dma)));
	AZ_WRITE_1(az, RIRBSIZE, rirbsize);
	az->rirb_size = entries;

	DPRINTF(("%s: RIRB allocation succeeded.\n", __func__));

	//rirbctl = AZ_READ_1(az, RIRBCTL);
	//AZ_WRITE_1(az, RIRBCTL, rirbctl & ~HDA_RIRBCTL_RINTCTL);

	/* reset the write pointer */
	rirbwp = AZ_READ_2(az, RIRBWP);
	AZ_WRITE_2(az, RIRBWP, rirbwp | HDA_RIRBWP_RIRBWPRST);

	/* clear the read pointer */
	az->rirb_rp = AZ_READ_2(az, RIRBWP) & HDA_RIRBWP_RIRBWP;
	DPRINTF(("%s: RIRBRP=%d, size=%d\n", __func__, az->rirb_rp, az->rirb_size));

	AZ_WRITE_2(az, RINTCNT, 1);

	/* Run! */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	AZ_WRITE_1(az, RIRBCTL, rirbctl | HDA_RIRBCTL_RIRBDMAEN | HDA_RIRBCTL_RINTCTL);
	return 0;
}

static int
azalia_set_command(const azalia_t *az, int caddr, nid_t nid, uint32_t control,
		   uint32_t param)
{
	corb_entry_t *corb;
	int  wp;
	uint32_t verb;
	uint16_t corbwp;

#ifdef DIAGNOSTIC
	if ((AZ_READ_1(az, CORBCTL) & HDA_CORBCTL_CORBRUN) == 0) {
		aprint_error("%s: CORB is not running.\n", XNAME(az));
		return -1;
	}
#endif
	verb = (caddr << 28) | (nid << 20) | (control << 8) | param;
	corbwp = AZ_READ_2(az, CORBWP);
	wp = corbwp & HDA_CORBWP_CORBWP;
	corb = (corb_entry_t*)az->corb_dma.addr;
	if (++wp >= az->corb_size)
		wp = 0;
	corb[wp] = verb;
	AZ_WRITE_2(az, CORBWP, (corbwp & ~HDA_CORBWP_CORBWP) | wp);
#if 0
	DPRINTF(("%s: caddr=%d nid=%d control=0x%x param=0x%x verb=0x%8.8x wp=%d\n",
		 __func__, caddr, nid, control, param, verb, wp));
#endif
	return 0;
}

static int
azalia_get_response(azalia_t *az, uint32_t *result)
{
	const rirb_entry_t *rirb;
	int i;
	uint16_t wp;

#ifdef DIAGNOSTIC
	if ((AZ_READ_1(az, RIRBCTL) & HDA_RIRBCTL_RIRBDMAEN) == 0) {
		aprint_error("%s: RIRB is not running.\n", XNAME(az));
		return -1;
	}
#endif
	for (i = 5000; i >= 0; i--) {
		wp = AZ_READ_2(az, RIRBWP) & HDA_RIRBWP_RIRBWP;
		if (az->rirb_rp != wp)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		aprint_error("%s: RIRB time out\n", XNAME(az));
		return ETIMEDOUT;
	}
	rirb = (rirb_entry_t*)az->rirb_dma.addr;
	if (++az->rirb_rp >= az->rirb_size)
		az->rirb_rp = 0;
	if (result != NULL)
		*result = rirb[az->rirb_rp].resp;
#if 0
	DPRINTF(("%s: rirbwp=%d rp=%d resp1=0x%8.8x resp2=0x%8.8x\n",
		 __func__, wp, az->rirb_rp, rirb[az->rirb_rp].resp,
		 rirb[az->rirb_rp].resp_ex));
#endif
#if 0
	for (i = 0; i < 16 /*az->rirb_size*/; i++) {
		DPRINTF(("rirb[%d] 0x%8.8x:0x%8.8x ", i, rirb[i].resp, rirb[i].resp_ex));
		if ((i % 2) == 1)
			DPRINTF(("\n"));
	}
#endif
	return 0;
}

static int
azalia_alloc_dmamem(azalia_t *az, size_t size, size_t align, azalia_dma_t *d)
{
	int err;
	int nsegs;

	d->size = size;
	err = bus_dmamem_alloc(az->dmat, size, align, 0, d->segments, 1,
	    &nsegs, BUS_DMA_NOWAIT);
	if (err)
		return err;
	if (nsegs != 1)
		goto free;
	err = bus_dmamem_map(az->dmat, d->segments, 1, size,
	    &d->addr, BUS_DMA_NOWAIT | BUS_DMA_COHERENT | BUS_DMA_NOCACHE);
	if (err)
		goto free;
	err = bus_dmamap_create(az->dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &d->map);
	if (err)
		goto unmap;
	err = bus_dmamap_load(az->dmat, d->map, d->addr, size,
	    NULL, BUS_DMA_NOWAIT);
	if (err)
		goto destroy;
	return 0;

destroy:
	bus_dmamap_destroy(az->dmat, d->map);
unmap:
	bus_dmamem_unmap(az->dmat, d->addr, size);
free:
	bus_dmamem_free(az->dmat, d->segments, 1);
	return err;
}

static int
azalia_free_dmamem(const azalia_t *az, azalia_dma_t* d)
{
	bus_dmamap_unload(az->dmat, d->map);
	bus_dmamap_destroy(az->dmat, d->map);
	bus_dmamem_unmap(az->dmat, d->addr, d->size);
	bus_dmamem_free(az->dmat, d->segments, 1);
	return 0;
}

/* ================================================================
 * HDA coodec functions
 * ================================================================ */

static int
azalia_codec_init(codec_t *this)
{
	uint32_t result, bits_rates;
	int err, addr, n, i;
	int assoc, npin, dac, chan, seq, nbits;
	int pindexes[HDA_MAX_CHANNELS];

	this->comresp = azalia_codec_comresp;

	addr = this->address;
	DPRINTF(("%s: information of codec[%d] follows:\n",
	    XNAME(this->az), addr));
	/* codec vendor/device */
	err = this->comresp(this, CORB_NID_ROOT, CORB_GET_PARAMETER,
	    COP_VENDOR_ID, &result);
	if (err)
		return err;
	aprint_normal("%s: vendor/device = 0x%4.4x/0x%4.4x\n",
	    XNAME(this->az), result >> 16, result & 0xffff);

	/* identify function nodes */
	err = this->comresp(this, CORB_NID_ROOT, CORB_GET_PARAMETER,
	    COP_SUBORDINATE_NODE_COUNT, &result);
	if (err)
		return err;
	this->nfunctions = COP_NSUBNODES(result);
	if (COP_NSUBNODES(result) <= 0) {
		aprint_error("%s: No function groups\n", XNAME(this->az));
		return -1;
	}
	n = COP_START_NID(result);
	DPRINTF(("%s: nidstart=%d #functions=%d\n",
	    __func__, n, this->nfunctions));
	this->audiofunc = -1;
	for (i = 0; i < this->nfunctions; i++) {
		err = this->comresp(this, n + i, CORB_GET_PARAMETER,
		    COP_FUNCTION_GROUP_TYPE, &result);
		if (err)
			continue;
		DPRINTF(("%s: FTYPE result = 0x%8.8x\n", __func__, result));
		if (COP_FTYPE(result) == COP_FTYPE_AUDIO) {
			this->audiofunc = n + i;
			break;	/* XXX multiple audio functions? */
		}
	}
	if (this->audiofunc < 0) {
		aprint_error("%s: codec[%d]: No audio functions\n",
		    XNAME(this->az), addr);
		return -1;
	}

	/* check widgets in the audio function */
	err = this->comresp(this, this->audiofunc,
	    CORB_GET_PARAMETER, COP_SUBORDINATE_NODE_COUNT, &result);
	if (err)
		return err;
	DPRINTF(("%s: There are %d widgets in the audio function.\n",
	   __func__, COP_NSUBNODES(result)));
	if (COP_START_NID(result) < 2) {
		aprint_error("%s: invalid node structure\n", XNAME(this->az));
		return -1;
	}
	this->nwidgets = COP_NSUBNODES(result);
	this->widgets = malloc(sizeof(widget_t) * this->nwidgets,
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (this->widgets == NULL) {
		aprint_error("%s: out of memory\n", XNAME(this->az));
		return ENOMEM;
	}
	n = COP_START_NID(result);
	for (i = 0; i < this->nwidgets; i++) {
		err = azalia_widget_init(&this->widgets[i], this, n + i);
		if (err)
			return err;
	}

	/* search pins for the lowest association */
	assoc = INT_MAX;
	for (i = 0; i < this->nwidgets; i++) {
		if (this->widgets[i].type != COP_AWTYPE_PIN_COMPLEX)
			continue;
		if ((this->widgets[i].d.pin.cap & COP_PINCAP_OUTPUT) == 0)
			continue;
		if (this->widgets[i].d.pin.association < assoc)
			assoc = this->widgets[i].d.pin.association;
	}
	if (assoc == INT_MAX) {
		aprint_error("%s: no pins\n", XNAME(this->az));
		return -1;
	}
	/* collect pins in the lowest association */
	/* XXX digital and non-digital pins are stored */
	for (seq = 0, npin = 0;
	    npin < HDA_MAX_CHANNELS && seq <= CORB_CD_SEQUENCE_MAX; seq++) {
		i = azalia_codec_find_pin(this, COP_PINCAP_OUTPUT, assoc, seq);
		if (i < 0)
			continue;
		pindexes[npin] = i;
		this->dacindexes[npin] = azalia_codec_find_dac(this, i, 0);
		if (this->dacindexes[npin] < 0) {
			aprint_error("%s: a pin 0x%x does not connect to any DAC.",
			    XNAME(this->az), this->widgets[i].nid);
			return -1;
		}
		npin++;
	}
	for (i = 0; i < npin; i++) {
		this->comresp(this, this->widgets[pindexes[i]].nid,
		    CORB_SET_PIN_WIDGET_CONTROL, CORB_PWC_OUTPUT | CORB_PWC_VREF_100, &result);
	}
	this->ndacindexes = npin;
	DPRINTF(("%s: DACs:", __func__));
	for (i = 0; i < npin; i++) {
		DPRINTF((" 0x%x", this->widgets[this->dacindexes[i]].nid));
	}
	DPRINTF(("\n"));

	/* construct audio_format */
	azalia_widget_print_audio(&this->widgets[this->dacindexes[0]], XNAME(this->az));
	bits_rates = this->widgets[this->dacindexes[0]].d.audio.bits_rates;
	nbits = 0;
	if (bits_rates & COP_PCM_B8)
		nbits++;
	if (bits_rates & COP_PCM_B16)
		nbits++;
	if (bits_rates & COP_PCM_B20)
		nbits++;
	if (bits_rates & COP_PCM_B24)
		nbits++;
	if (bits_rates & COP_PCM_B32)
		nbits++;
	if (nbits == 0) {
		aprint_error("%s: invalid PCM format: 0x%8.8x\n",
		    XNAME(this->az), bits_rates);
		return -1;
	}
	this->formats = malloc(sizeof(struct audio_format) * npin * nbits,
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (this->formats == NULL) {
		aprint_error("%s: out of memory in %s\n",
		    XNAME(this->az), __func__);
		return ENOMEM;
	}
	for (dac = 0; dac < npin; dac++) {
		for (chan = 0, i = 0; i <= dac; i++)
			chan += this->widgets[this->dacindexes[dac]].widgetcap
			    & COP_AWCAP_STEREO ? 2 : 1;
		if (bits_rates & COP_PCM_B8)
			azalia_codec_add_format(this, chan, 8, 16, bits_rates);
		if (bits_rates & COP_PCM_B16)
			azalia_codec_add_format(this, chan, 16, 16, bits_rates);
		if (bits_rates & COP_PCM_B20)
			azalia_codec_add_format(this, chan, 20, 32, bits_rates);
		if (bits_rates & COP_PCM_B24)
			azalia_codec_add_format(this, chan, 24, 32, bits_rates);
		if (bits_rates & COP_PCM_B32)
			azalia_codec_add_format(this, chan, 32, 32, bits_rates);
	}

	err = auconv_create_encodings(this->formats, this->nformats,
	    &this->encodings);
	if (err)
		return err;

	/* XXX unmute all of amplifiers */
	DPRINTF(("%s: unmute all widgets\n", __func__));
	for (i = 0; i < this->nwidgets; i++) {
		if (this->widgets[i].widgetcap & COP_AWCAP_INAMP) {
			err = this->comresp(this, this->widgets[i].nid,
			    CORB_GET_PARAMETER, COP_INPUT_AMPLIFIER_CAPABILITIES, &result);
			for (n = 0; n < this->widgets[i].nconnections; n++) {
			err = this->comresp(this, this->widgets[i].nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, CORB_AGM_INPUT |
			    CORB_AGM_LEFT | CORB_AGM_RIGHT | (n << 8) |
			    (result & 0x7f), &result);
			}
		}
		if (this->widgets[i].widgetcap & COP_AWCAP_OUTAMP) {
			err = this->comresp(this, this->widgets[i].nid,
			    CORB_GET_PARAMETER, COP_OUTPUT_AMPLIFIER_CAPABILITIES, &result);
			err = this->comresp(this, this->widgets[i].nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, CORB_AGM_OUTPUT |
			    CORB_AGM_LEFT | CORB_AGM_RIGHT | (result & 0x7f), &result);
		}
	}
	DPRINTF(("%s: done.\n", __func__));
	return 0;
}

static void
azalia_codec_add_format(codec_t *this, int chan, int valid, int prec, uint32_t rates)
{
	struct audio_format *f;

	f = &this->formats[this->nformats++];
	f->mode = AUMODE_PLAY;	/* XXX */
	f->encoding = AUDIO_ENCODING_SLINEAR_LE;
	if (valid == 8 && prec == 8)
		f->encoding = AUDIO_ENCODING_ULINEAR_LE;
	f->validbits = valid;
	f->precision = prec;
	f->channels = chan;
	switch (chan) {
	case 1:
		f->channel_mask = AUFMT_MONAURAL;
		break;
	case 2:
		f->channel_mask = AUFMT_STEREO;
		break;
	case 4:
		f->channel_mask = AUFMT_SURROUND4;
		break;
	case 6:
		f->channel_mask = AUFMT_DOLBY_5_1;
		break;
	case 8:
		f->channel_mask = AUFMT_DOLBY_5_1
		    | AUFMT_SIDE_LEFT | AUFMT_SIDE_RIGHT;
		break;
	default:
		f->channel_mask = 0;
	}
	if (rates & COP_PCM_R80)
		f->frequency[f->frequency_type++] = 8000;
	if (rates & COP_PCM_R110)
		f->frequency[f->frequency_type++] = 11025;
	if (rates & COP_PCM_R160)
		f->frequency[f->frequency_type++] = 16000;
	if (rates & COP_PCM_R220)
		f->frequency[f->frequency_type++] = 22050;
	if (rates & COP_PCM_R320)
		f->frequency[f->frequency_type++] = 32000;
	if (rates & COP_PCM_R441)
		f->frequency[f->frequency_type++] = 44100;
	if (rates & COP_PCM_R480)
		f->frequency[f->frequency_type++] = 48000;
	if (rates & COP_PCM_R882)
		f->frequency[f->frequency_type++] = 88200;
	if (rates & COP_PCM_R960)
		f->frequency[f->frequency_type++] = 96000;
	if (rates & COP_PCM_R1764)
		f->frequency[f->frequency_type++] = 176400;
	if (rates & COP_PCM_R1920)
		f->frequency[f->frequency_type++] = 192000;
	if (rates & COP_PCM_R3840)
		f->frequency[f->frequency_type++] = 384000;
}

static int
azalia_codec_find_pin(const codec_t *this, uint32_t dir, int assoc, int seq)
{
	int i;

	for (i = 0; i < this->nwidgets; i++) {
		if (this->widgets[i].type != COP_AWTYPE_PIN_COMPLEX)
			continue;
		if ((this->widgets[i].d.pin.cap & dir) == 0)
			continue;
		if (this->widgets[i].d.pin.association != assoc)
			continue;
		if (this->widgets[i].d.pin.sequence == seq) {
			return i;
		}
	}
	return -1;
}

static int
azalia_codec_find_dac(const codec_t *this, int index, int depth)
{
	const widget_t *w;
	int i, j, ret;

	w = &this->widgets[index];
	if (w->type == COP_AWTYPE_AUDIO_OUTPUT) {
		DPRINTF(("%s: DAC: nid=0x%x index=%d\n",
		    __func__, w->nid, index));
		return index;
	}
	if (++depth > 50) {
		return -1;
	}
	if (w->selected >= 0) {
		j = azalia_codec_nid2index(this, w->connections[w->selected]);
		ret = azalia_codec_find_dac(this, j, depth);
		if (ret >= 0) {
			DPRINTF(("%s: DAC path: nid=0x%x index=%d\n",
			    __func__, w->nid, index));
			return ret;
		}
	}
	for (i = 0; i < w->nconnections; i++) {
		j = azalia_codec_nid2index(this, w->connections[i]);
		ret = azalia_codec_find_dac(this, j, depth);
		if (ret >= 0) {
			DPRINTF(("%s: DAC path: nid=0x%x index=%d\n",
			    __func__, w->nid, index));
			return ret;
		}
	}
	return -1;
}

static int
azalia_codec_nid2index(const codec_t *this, nid_t nid)
{
	int i;

	i = nid;
	if (i >= this->nwidgets)
		i = this->nwidgets - 1;
	for (; i >= 0; i--) {
		if (this->widgets[i].nid == nid)
			return i;
	}
	aprint_error("%s: internal error: nid=0x%x is not found.\n",
		     __func__, nid);
	return -1;
}

static int
azalia_codec_comresp(const codec_t *codec, nid_t nid, uint32_t control,
		     uint32_t param, uint32_t* result)
{
	int err;

	err = azalia_set_command(codec->az, codec->address, nid, control, param);
	if (err)
		return err;
	return azalia_get_response(codec->az, result);
}

static int
azalia_codec_connect_stream(codec_t *this, int dir, uint16_t fmt, int number)
{
	uint32_t result;
	int i, err, startchan;
	nid_t nid;

	/* XXX */

	DPRINTF(("%s: fmt=0x%4.4x number=%d\n", __func__, fmt, number));
	err = 0;
	if (dir == AUMODE_RECORD) {
		printf("%s: not implemented for AUMODE_RECORD\n", __func__);
		return -1;
	}
	startchan = 0;
	i = this->dacindexes[0];
	nid = this->widgets[i].nid;
	err = this->comresp(this, nid, CORB_SET_CONVERTER_FORMAT, fmt, &result);
	if (err)
		goto exit;
	err = this->comresp(this, nid, CORB_SET_CONVERTER_STREAM_CHANNEL,
	    (number << 4) | startchan, &result);
	if (err)
		goto exit;
	startchan += this->widgets[i].widgetcap & COP_AWCAP_STEREO ? 2 : 1;

exit:
	DPRINTF(("%s: leave with %d\n", __func__, err));
	return err;
}

/* ================================================================
 * HDA widget functions
 * ================================================================ */

static int
azalia_widget_init(widget_t *this, const codec_t *codec, nid_t nid)
{
#define FLAGBUFLEN	256
	char flagbuf[FLAGBUFLEN];
	uint32_t result;
	int err;

	err = codec->comresp(codec, nid, CORB_GET_PARAMETER,
	    COP_AUDIO_WIDGET_CAPABILITIES, &result);
	if (err)
		return err;
	this->nid = nid;
	this->widgetcap = result;
	this->type = COP_AWCAP_TYPE(result);
	bitmask_snprintf(result, "\20\014LRSWAP\013POWER\012DIGITAL"
	    "\011CONLIST\010UNSOL\07PROC\06STRIPE\05FORMATOV\04AMAPOV\03OUTAMP"
	    "\02INAMP\01STEREO", flagbuf, FLAGBUFLEN);
	DPRINTF(("%s: node[0x%x]: ", XNAME(codec->az), nid));
	switch (this->type) {
	case COP_AWTYPE_AUDIO_OUTPUT:
		DPRINTF(("AUDIO_OUTPUT; %s\n", flagbuf));
		azalia_widget_init_audio(this, codec);
		break;
	case COP_AWTYPE_AUDIO_INPUT:
		DPRINTF(("AUDIO_INPUT; %s\n", flagbuf));
		azalia_widget_init_audio(this, codec);
		break;
	case COP_AWTYPE_AUDIO_MIXER:
		DPRINTF(("MIXER; %s\n", flagbuf));
		break;
	case COP_AWTYPE_AUDIO_SELECTOR:
		DPRINTF(("SELECTOR; %s\n", flagbuf));
		break;
	case COP_AWTYPE_PIN_COMPLEX:
		DPRINTF(("PIN; %s\n", flagbuf));
		azalia_widget_init_pin(this, codec);
		break;
	case COP_AWTYPE_POWER:
		DPRINTF(("POWER; %s\n", flagbuf));
		break;
	case COP_AWTYPE_VOLUME_KNOB:
		DPRINTF(("VOLUME_KNOB; %s\n", flagbuf));
		break;
	case COP_AWTYPE_BEEP_GENERATOR:
		DPRINTF(("BEEP; %s\n", flagbuf));
		break;
	default:
		DPRINTF(("unknown; %s\n", flagbuf));
		break;
	}
	azalia_widget_init_connection(this, codec);
	return 0;
}

static int
azalia_widget_init_audio(widget_t *this, const codec_t *codec)
{
	char leadbuf[FLAGBUFLEN];
	uint32_t result;
	int err;

	/* check audio format */
	err = codec->comresp(codec, this->nid,
	    CORB_GET_PARAMETER, COP_STREAM_FORMATS, &result);
	if (err)
		return err;
	this->d.audio.encodings = result;
	if ((result & COP_STREAM_FORMAT_PCM) == 0) {
		aprint_error("%s: No PCM support\n", XNAME(codec->az));
		return -1;
	}
	err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER, COP_PCM, &result);
	if (err)
		return err;
	this->d.audio.bits_rates = result;
	snprintf(leadbuf, FLAGBUFLEN, "%s: node[0x%x]", XNAME(codec->az), this->nid);
#ifdef AZALIA_DEBUG
	azalia_widget_print_audio(this, leadbuf);
#endif
	return 0;
}

static int
azalia_widget_print_audio(const widget_t *this, const char *lead)
{
	char flagbuf[FLAGBUFLEN];

	bitmask_snprintf(this->d.audio.encodings, "\20\3AC3\2FLOAT32\1PCM",
	    flagbuf, FLAGBUFLEN);
	aprint_normal("%s: encodings=%s\n", lead, flagbuf);
	bitmask_snprintf(this->d.audio.bits_rates, "\20\x15""32bit\x14""24bit\x13""20bit"
	    "\x12""16bit\x11""8bit""\x0c""384kHz\x0b""192kHz\x0a""176.4kHz"
	    "\x09""96kHz\x08""88.2kHz\x07""48kHz\x06""44.1kHz\x05""32kHz\x04"
	    "22.05kHz\x03""16kHz\x02""11.025kHz\x01""8kHz",
	    flagbuf, FLAGBUFLEN);
	aprint_normal("%s: PCM formats=%s\n", lead, flagbuf);
	return 0;
}


static int
azalia_widget_init_pin(widget_t *this, const codec_t *codec)
{
#ifdef AZALIA_DEBUG
	static const char *COLORS[16] = {
		"unknown", "black", "gray", "blue",
		"green", "red", "orange", "yellow",
		"purple", "pink", "0xa", "0xb",
		"0xc", "0xd", "white", "other"};
	static const char *DEVICES[16] = {
		"line-out", "speaker", "headphone", "cd",
		"SPDIF-out", "digital-out", "modem-line", "modem-handset",
		"line-in", "aux", "mic-in", "telephony",
		"SPDIF-in", "digital-in", "0xe", "other"};
#endif
	char flagbuf[FLAGBUFLEN];
	uint32_t result;
	int err;

	err = codec->comresp(codec, this->nid, CORB_GET_CONFIGURATION_DEFAULT,
	    0, &result);
	if (err)
		return err;
	this->d.pin.config = result;
	this->d.pin.sequence = CORB_CD_SEQUENCE(result);
	this->d.pin.association = CORB_CD_ASSOCIATION(result);
	this->d.pin.color = CORB_CD_COLOR(result);
	this->d.pin.device = CORB_CD_DEVICE(result);
	DPRINTF(("%s: node[0x%x]: pin configuration; device=%s "
		 "color=%s assoc=%d seq=%d",
		 XNAME(codec->az), this->nid,
		 DEVICES[CORB_CD_DEVICE(result)], COLORS[CORB_CD_COLOR(result)],
		 CORB_CD_ASSOCIATION(result), CORB_CD_SEQUENCE(result)));
	err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER,
	    COP_PIN_CAPABILITIES, &result);
	if (err)
		return err;
	this->d.pin.cap = result;
	bitmask_snprintf(result, "\20\021EAPD\07BALANCE\06INPUT\05OUTPUT"
	    "\04HEADPHONE\03PRESENCE\02TRIGGER\01IMPEDANCE", flagbuf, FLAGBUFLEN);
	DPRINTF((" cap=%s\n", flagbuf));
	return 0;
}

static int
azalia_widget_init_connection(widget_t *this, const codec_t *codec)
{
	uint32_t result;
	int err;
	boolean_t longform;
	int length, i;

	this->selected = -1;
	if ((this->widgetcap & COP_AWCAP_CONLIST) == 0)
		return 0;

	err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER,
	    COP_CONNECTION_LIST_LENGTH, &result);
	if (err)
		return err;
	longform = (result & COP_CLL_LONG) != 0;
	length = COP_CLL_LENGTH(result);
	if (length == 0)
		return 0;
	this->nconnections = length;
	this->connections = malloc(sizeof(nid_t) * length, M_DEVBUF, M_NOWAIT);
	if (this->connections == NULL) {
		aprint_error("%s: out of memory\n", XNAME(codec->az));
		return ENOMEM;
	}
	if (longform) {
		for (i = 0; i < length; i += 2) {
			err = codec->comresp(codec, this->nid,
			    CORB_GET_CONNECTION_LIST_ENTRY, i, &result);
			if (err)
				return err;
			this->connections[i] = CORB_CLE_LONG_0(result);
			this->connections[i+1] = CORB_CLE_LONG_1(result);
		}
	} else {
		for (i = 0; i < length; i += 4) {
			err = codec->comresp(codec, this->nid,
			    CORB_GET_CONNECTION_LIST_ENTRY, i, &result);
			if (err)
				return err;
			this->connections[i] = CORB_CLE_SHORT_0(result);
			this->connections[i+1] = CORB_CLE_SHORT_1(result);
			this->connections[i+2] = CORB_CLE_SHORT_2(result);
			this->connections[i+3] = CORB_CLE_SHORT_3(result);
		}
	}
	if (length > 0) {
		DPRINTF(("%s: node[0x%x]: connections=0x%x", XNAME(codec->az),
			 this->nid, this->connections[0]));
		for (i = 1; i < length; i++) {
			DPRINTF((",0x%x", this->connections[i]));
		}

		err = codec->comresp(codec, this->nid,
		    CORB_GET_CONNECTION_SELECT_CONTROL, 0, &result);
		if (err)
			return err;
		this->selected = CORB_CSC_INDEX(result);
		DPRINTF(("; selected=0x%x\n", this->connections[result]));

	}
	return 0;
}

static int
azalia_widget_nid2clindex(const widget_t *this, nid_t nid)
{
	int i;

	for (i = 0; i < this->nconnections; i++) {
		if (this->connections[i] == nid)
			return i;
	}
	return -1;
}

int
azalia_widget_set_connection(const widget_t *this, const codec_t *codec, int nid)
{
	int clindex;

	clindex = azalia_widget_nid2clindex(this, nid);
	if (clindex < 0) {
		aprint_error("%s: invalid nid: %d\n", XNAME(codec->az), nid);
		return -1;
	}
	return codec->comresp(codec, this->nid,
	    CORB_SET_CONNECTION_SELECT_CONTROL, clindex, NULL);
}

/* ================================================================
 * MI audio functions
 * ================================================================ */

static int
azalia_query_encoding(void *v, audio_encoding_t *enc)
{
	azalia_t *az;
	codec_t *codec;

	az = v;
	codec = &az->codecs[0];
	return auconv_query_encoding(codec->encodings, enc);
}

static int
azalia_set_params(void *v, int smode, int umode, audio_params_t *p,
    audio_params_t *r, stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	azalia_t *az;
	codec_t *codec;
	int index;

	az = v;
	codec = &az->codecs[0];
	if (smode & AUMODE_RECORD && r != NULL) {
		index = auconv_set_converter(codec->formats, codec->nformats,
		    AUMODE_RECORD, r, TRUE, rfil);
		if (index < 0)
			return 0/* XXX EINVAL */;
	}
	if (smode & AUMODE_PLAY && p != NULL) {
		index = auconv_set_converter(codec->formats, codec->nformats,
		    AUMODE_PLAY, p, TRUE, pfil);
		if (index < 0)
			return EINVAL;
	}
	return 0;
}

static int
azalia_round_blocksize(void *v, int blk, int mode, const audio_params_t *param)
{
	azalia_t *az;
	size_t size;

	blk &= ~0x7f;		/* must be multiple of 128 */
	if (blk <= 0)
		blk = 128;
	/* number of blocks must be <= HDA_BDL_MAX */
	az = v;
	size = mode == AUMODE_PLAY ? az->pstream.buffer.size : az->rstream.buffer.size;
#ifdef DIAGNOSTIC
	if (size <= 0) {
		aprint_error("%s: size is 0", __func__);
		return 256;
	}
#endif
	if (size > HDA_BDL_MAX * blk) {
		blk = size / HDA_BDL_MAX;
		if (blk & 0x7f)
			blk = (blk + 0x7f) & ~0x7f;
	}
	DPRINTF(("%s: resultant block size = %d\n", __func__, blk));
	return blk;
}

static int
azalia_halt_output(void *v)
{
	azalia_t *az;
	stream_t *str;
	uint16_t ctl;

	az = v;
	str = &az->pstream;
	ctl = STR_READ_2(az, str, CTL);
	ctl &= ~(HDA_SD_CTL_DEIE | HDA_SD_CTL_FEIE | HDA_SD_CTL_IOCE | HDA_SD_CTL_RUN);
	STR_WRITE_2(az, str, CTL, ctl);
	AZ_WRITE_1(az, INTCTL, AZ_READ_1(az, INTCTL) & ~str->intr_bit);
	return 0;
}

static int
azalia_halt_input(void *v)
{
	aprint_error("%s: NOT IMPLEMENTED\n", __func__); /* XXX */
	return ENXIO;
}

static int
azalia_getdev(void *v, struct audio_device *dev)
{
	azalia_t *az;

	az = v;
	strlcpy(dev->name, "Azalia", MAX_AUDIO_DEV_LEN);
	snprintf(dev->version, MAX_AUDIO_DEV_LEN,
	    "%d.%d", AZ_READ_1(az, VMAJ), AZ_READ_1(az, VMIN));
	strlcpy(dev->config, XNAME(az), MAX_AUDIO_DEV_LEN);
	return 0;
}

static int
azalia_set_port(void *v, mixer_ctrl_t *mc)
{
	aprint_error("%s: NOT IMPLEMENTED\n", __func__); /* XXX */
	return ENXIO;
}

static int
azalia_get_port(void *v, mixer_ctrl_t *mc)
{
	aprint_error("%s: NOT IMPLEMENTED\n", __func__); /* XXX */
	return ENXIO;
}

static int
azalia_query_devinfo(void *v, mixer_devinfo_t *mdev)
{
	aprint_error("%s: NOT IMPLEMENTED\n", __func__); /* XXX */
	return ENXIO;
}

static void *
azalia_allocm(void *v, int dir, size_t size, struct malloc_type *pool, int flags)
{
	azalia_t *az;
	stream_t *stream;
	int err;

	az = v;
	stream = dir == AUMODE_PLAY ? &az->pstream : &az->rstream;
	err = azalia_alloc_dmamem(az, size, 128, &stream->buffer);
	if (err)
		return NULL;
	return stream->buffer.addr;
}

static void
azalia_freem(void *v, void *addr, struct malloc_type *pool)
{
	azalia_t *az;
	stream_t *stream;

	az = v;
	if (addr == az->pstream.buffer.addr) {
		stream = &az->pstream;
	} else if (addr == az->rstream.buffer.addr) {
		stream = &az->rstream;
	} else {
		return;
	}
	azalia_free_dmamem(az, &stream->buffer);
}

static size_t
azalia_round_buffersize(void *v, int dir, size_t size)
{
	size &= ~0x7f;		/* must be multiple of 128 */
	if (size <= 0)
		size = 128;
	return size;
}

static int
azalia_get_props(void *v)
{
	return AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

static int
azalia_trigger_output(void *v, void *start, void *end, int blk,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	azalia_t *az;
	stream_t *str;
	bdlist_entry_t *bdlist;
	bus_addr_t dmaaddr;
	int err, index;
	uint16_t fmt, ctl;
	uint8_t ctl2, intctl;

	DPRINTF(("%s: this=%p start=%p end=%p blk=%d {enc=%u %uch %u/%ubit %uHz}\n",
	    __func__, v, start, end, blk, param->encoding, param->channels,
	    param->validbits, param->precision, param->sample_rate));

	az = v;
	str = &az->pstream;
	str->intr = intr;
	str->intr_arg = arg;

	/* setup BDL */
	dmaaddr = AZALIA_DMA_DMAADDR(&str->buffer);
	str->dmaend = dmaaddr + ((caddr_t)end - (caddr_t)start);
	bdlist = (bdlist_entry_t*)str->bdlist.addr;
	for (index = 0; index < HDA_BDL_MAX; index++) {
		bdlist[index].low = dmaaddr;
		bdlist[index].high = PTR_UPPER32(dmaaddr);
		bdlist[index].length = blk;
		bdlist[index].flags = BDLIST_ENTRY_IOC;
		dmaaddr += blk;
		if (dmaaddr >= str->dmaend) {
			index++;
			break;
		}
	}
	/* The BDL covers the whole of the buffer. */
	str->dmanext = AZALIA_DMA_DMAADDR(&str->buffer);

	dmaaddr = AZALIA_DMA_DMAADDR(&str->bdlist);
	STR_WRITE_4(az, str, BDPL, dmaaddr);
	STR_WRITE_4(az, str, BDPU, PTR_UPPER32(dmaaddr));
	STR_WRITE_2(az, str, LVI, (index - 1) & HDA_SD_LVI_LVI);
	ctl2 = STR_READ_1(az, str, CTL2);
	STR_WRITE_1(az, str, CTL2,
	    (ctl2 & ~HDA_SD_CTL2_STRM) | (str->number << HDA_SD_CTL2_STRM_SHIFT));
	STR_WRITE_4(az, str, CBL, ((caddr_t)end - (caddr_t)start));

	err = azalia_params2fmt(param, &fmt);
	if (err)
		return EINVAL;
	STR_WRITE_2(az, str, FMT, fmt);
	err = azalia_codec_connect_stream(&az->codecs[0], AUMODE_PLAY,
	    fmt, str->number);
	if (err)
		return EINVAL;

	intctl = AZ_READ_1(az, INTCTL);
	intctl |= str->intr_bit;
	AZ_WRITE_1(az, INTCTL, intctl);

	ctl = STR_READ_2(az, str, CTL);
	ctl |= ctl | HDA_SD_CTL_DEIE | HDA_SD_CTL_FEIE | HDA_SD_CTL_IOCE | HDA_SD_CTL_RUN;
	STR_WRITE_2(az, str, CTL, ctl);
	return 0;
}

static int
azalia_trigger_input(void *v, void *start, void *end, int blk,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	DPRINTF(("%s: this=%p start=%p end=%p blk=%d {enc=%u %uch %u/%ubit %uHz}\n",
	    __func__, v, start, end, blk, param->encoding, param->channels,
	    param->validbits, param->precision, param->sample_rate));
	aprint_error("%s: NOT IMPLEMENTED\n", __func__); /* XXX */
	return ENXIO;
}

/* --------------------------------
 * helpers for MI audio functions
 * -------------------------------- */
static int
azalia_params2fmt(const audio_params_t *param, uint16_t *fmt)
{
	uint16_t ret;

	ret = 0;
#ifdef DIAGNOSTIC
	if (param->channels > HDA_MAX_CHANNELS) {
		aprint_error("%s: too many channels: %u\n", __func__, param->channels);
		return EINVAL;
	}
#endif
	ret |= param->channels - 1;

	switch (param->validbits) {
	case 8:
		ret |= HDA_SD_FMT_BITS_8_16;
		break;
	case 16:
		ret |= HDA_SD_FMT_BITS_16_16;
		break;
	case 20:
		ret |= HDA_SD_FMT_BITS_20_32;
		break;
	case 24:
		ret |= HDA_SD_FMT_BITS_24_32;
		break;
	case 32:
		ret |= HDA_SD_FMT_BITS_32_32;
		break;
	default:
		aprint_error("%s: invalid validbits: %u\n", __func__, param->validbits);
	}

	if (param->sample_rate == 384000) {
		aprint_error("%s: invalid sample_rate: %u\n", __func__, param->sample_rate);
		return EINVAL;
	} else if (param->sample_rate == 192000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X4 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 176400) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X4 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 96000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 88200) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 48000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 44100) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 32000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY3;
	} else if (param->sample_rate == 22050) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY2;
	} else if (param->sample_rate == 16000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY3;
	} else if (param->sample_rate == 11025) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY4;
	} else if (param->sample_rate == 8000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY6;
	} else {
		aprint_error("%s: invalid sample_rate: %u\n", __func__, param->sample_rate);
		return EINVAL;
	}
	*fmt = ret;
	return 0;
}

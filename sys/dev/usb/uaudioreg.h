/*	$NetBSD: uaudioreg.h,v 1.1 1999/09/09 12:28:26 augustss Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
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

#define UAUDIO_VERSION		0x100

#define UDESC_CS_DEVICE		0x21
#define UDESC_CS_CONFIG		0x22
#define UDESC_CS_STRING		0x23
#define UDESC_CS_INTERFACE	0x24
#define UDESC_CS_ENDPOINT	0x25

#define UDESCSUB_AC_HEADER	1
#define UDESCSUB_AC_INPUT	2
#define UDESCSUB_AC_OUTPUT	3
#define UDESCSUB_AC_MIXER	4
#define UDESCSUB_AC_SELECTOR	5
#define UDESCSUB_AC_FEATURE	6
#define UDESCSUB_AC_PROCESSING	7
#define UDESCSUB_AC_EXTENSION	8

/* The first fields are identical to usb_endpoint_descriptor_t */
typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bEndpointAddress;
	uByte		bmAttributes;
	uWord		wMaxPacketSize;
	uByte		bInterval;
	/* 
	 * The following two entries are only used by the Audio Class.
	 * And according to the specs the Audio Class is the only one
	 * allowed to extend the endpoint descriptor.
	 * Who knows what goes on in the minds of the people in the USB
	 * standardization?  :-(
	 */
	uByte		bRefresh;
	uByte		bSynchAddress;
} usb_endpoint_descriptor_audio_t;

struct usb_audio_control_descriptor {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uWord		bcdADC;
	uWord		wTotalLength;
	uByte		bInCollection;
	uByte		baInterfaceNr[1];
};

struct usb_audio_streaming_interface_descriptor {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bTerminalLink;
	uByte		bDelay;
	uWord		wFormatTag;
};

struct usb_audio_streaming_endpoint_descriptor {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bmAttributes;
	uByte		bLockDelayUnits;
	uWord		wLockDelay;
};

struct usb_audio_streaming_type1_descriptor {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bFormatType;
	uByte		bNrChannels;
	uByte		bSubFrameSize;
	uByte		bBitResolution;
	uByte		bSamFreqType;
#define UA_SAMP_CONTNUOUS 0
	uByte		tSamFreq[3*2]; /* room for low and high */
#define UA_GETSAMP(p, n) ((p)->tSamFreq[(n)*3+0] | ((p)->tSamFreq[(n)*3+1] << 8) | ((p)->tSamFreq[(n)*3+2] << 16))
#define UA_SAMP_LO(p) UA_GETSAMP(p, 0)
#define UA_SAMP_HI(p) UA_GETSAMP(p, 1)
};

struct usb_audio_cluster {
	uByte		bNrChannels;
	uWord		wChannelConfig;
	uByte		iChannelNames;
};

/* UDESCSUB_AC_INPUT */
struct usb_audio_input_terminal {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bTerminalId;
	uWord		wTerminalType;
	uByte		bAssocTerminal;
	uByte		bNrChannels;
	uWord		wChannelConfig;
	uByte		iChannelNames;
	uByte		iTerminal;
};

/* UDESCSUB_AC_OUTPUT */
struct usb_audio_output_terminal {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bTerminalId;
	uWord		wTerminalType;
	uByte		bAssocTerminal;
	uByte		bSourceId;
	uByte		iTerminal;
};

/* UDESCSUB_AC_MIXER */
struct usb_audio_mixer_unit {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bUnitId;
	uByte		bNrInPins;
	uByte		baSourceId[255]; /* length is really bNrInPins */
	/* struct usb_audio_mixer_unit_1 */
};
struct usb_audio_mixer_unit_1 {
	uByte		bNrChannels;
	uWord		wChannelConfig;
	uByte		iChannelNames;
	uByte		bmControls[255];
	/*uByte		iMixer;*/
};

/* UDESCSUB_AC_SELECTOR */
struct usb_audio_selector_unit {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bUnitId;
	uByte		bNrInPins;
	uByte		baSourceId[255];
	/* uByte	iSelector; */
};

/* UDESCSUB_AC_FEATURE */
struct usb_audio_feature_unit {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bUnitId;
	uByte		bSourceId;
	uByte		bControlSize;
	uByte		bmaControls[255]; /* size for more than enough */
	/* uByte	iFeature; */
};

/* UDESCSUB_AC_PROCESSING */
struct usb_audio_processing_unit {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bUnitId;
	uWord		wProcessType;
	uByte		bNrInPins;
	uByte		baSourceId[255];
	/* struct usb_audio_processing_unit_1 */
};
struct usb_audio_processing_unit_1{
	uByte		bNrChannels;
	uWord		wChannelConfig;
	uByte		iChannelNames;
	uByte		bControlSize;
	uByte		bmControls[255];
	/*uByte		iProcessing;*/
};

/* UDESCSUB_AC_EXTENSION */
struct usb_audio_extension_unit {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bUnitId;
	uWord		wExtensionCode;
	uByte		bNrInPins;
	uByte		baSourceId[255];
	/* struct usb_audio_extension_unit_1 */
};
struct usb_audio_extension_unit_1 {
	uByte		bNrChannels;
	uWord		wChannelConfig;
	uByte		iChannelNames;
	uByte		bControlSize;
	uByte		bmControls[255];
#define UA_EXT_ENABLE 0
	/*uByte		iExtension;*/
};

#define UAT_STREAM 0x0101

#define SET_CUR 0x01
#define GET_CUR 0x81
#define SET_MIN 0x02
#define GET_MIN 0x82
#define SET_MAX 0x03
#define GET_MAX 0x83
#define SET_RES 0x04
#define GET_RES 0x84
#define SET_MEM 0x05
#define GET_MEM 0x85
#define GET_STAT 0xff

#define MUTE_CONTROL	0x01
#define VOLUME_CONTROL	0x02
#define BASS_CONTROL	0x03
#define MID_CONTROL	0x04
#define TREBLE_CONTROL	0x05
#define GRAPHIC_EQUALIZER_CONTROL	0x06
#define AGC_CONTROL	0x07
#define DELAY_CONTROL	0x08
#define BASS_BOOST_CONTROL 0x09
#define LOUDNESS_CONTROL 0x0a

#define FU_MASK(u) (1 << ((u)-1))

#define MASTER_CHAN 0

#define AS_GENERAL 1
#define FORMAT_TYPE 2
#define FORMAT_SPECIFIC 3

#define PCM 1
#define PCM8 2
#define IEEE_FLOAT 3
#define ALAW 4
#define MULAW 5

#define SAMPLING_FREQ_CONTROL 0x01

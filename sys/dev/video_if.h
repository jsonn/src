/* $NetBSD: video_if.h,v 1.5.16.2 2009/05/04 08:12:33 yamt Exp $ */

/*
 * Copyright (c) 2008 Patrick Mahoney <pat@polycrystal.org>
 * All rights reserved.
 *
 * This code was written by Patrick Mahoney (pat@polycrystal.org) as
 * part of Google Summer of Code 2008.
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
 * This ia a Video4Linux 2 compatible /dev/video driver for NetBSD
 *
 * See http://v4l2spec.bytesex.org/ for Video4Linux 2 specifications
 */

#ifndef _SYS_DEV_VIDEO_IF_H_
#define _SYS_DEV_VIDEO_IF_H_

#include <sys/types.h>
#include <sys/videoio.h>

#if defined(_KERNEL_OPT)
#include "video.h"

#if (NVIDEO == 0)
#error "No 'video* at videobus?' configured"
#endif

#endif	/* _KERNEL_OPT */

struct video_softc;

/* Controls provide a way to query and set controls in the camera
 * hardware.  The control structure is the primitive unit.  Control
 * groups are arrays of controls that must be set together (e.g. pan
 * direction and pan speed).  Control descriptors describe a control
 * including minimum and maximum values, read-only state, etc.  A
 * control group descriptor is an array of control descriptors
 * corresponding to a control group array of controls.
 *
 * A control_group is made up of multiple controls meant to be set
 * together and is identified by a 16 bit group_id.  Each control is
 * identified by a group_id and a control_id.  Controls that are the
 * sole member of a control_group may ignore the control_id or
 * redundantly have the control_id equal to the group_id.
 *
 * The hardware driver only handles control_group's, many of which
 * will only have a single control.
 *
 * Some id's are defined here (closely following the USB Video Class
 * controls) with room for unspecified extended controls.  These id's
 * may be used for group_id's or control_id's as appropriate.
 */

enum video_control_id {
	VIDEO_CONTROL_UNDEFINED,
	/* camera hardware */
	VIDEO_CONTROL_SCANNING_MODE,
	VIDEO_CONTROL_AE_MODE,
	VIDEO_CONTROL_EXPOSURE_TIME_ABSOLUTE,
	VIDEO_CONTROL_EXPOSURE_TIME_RELATIVE,
	VIDEO_CONTROL_FOCUS_ABSOLUTE,
	VIDEO_CONTROL_FOCUS_RELATIVE,
	VIDEO_CONTROL_IRIS_ABSOLUTE,
	VIDEO_CONTROL_IRIS_RELATIVE,
	VIDEO_CONTROL_ZOOM_ABSOLUTE,
	VIDEO_CONTROL_ZOOM_RELATIVE,
	VIDEO_CONTROL_PANTILT_ABSOLUTE,
	VIDEO_CONTROL_PANTILT_RELATIVE,
	VIDEO_CONTROL_ROLL_ABSOLUTE,
	VIDEO_CONTROL_ROLL_RELATIVE,
	VIDEO_CONTROL_PRIVACY,
	/* video processing */
	VIDEO_CONTROL_BACKLIGHT_COMPENSATION,
	VIDEO_CONTROL_BRIGHTNESS,
	VIDEO_CONTROL_CONTRAST,
	VIDEO_CONTROL_GAIN,
	VIDEO_CONTROL_GAIN_AUTO, /* not in UVC */
	VIDEO_CONTROL_POWER_LINE_FREQUENCY,
	VIDEO_CONTROL_HUE,
	VIDEO_CONTROL_SATURATION,
	VIDEO_CONTROL_SHARPNESS,
	VIDEO_CONTROL_GAMMA,
	/* Generic WHITE_BALANCE controls applies to whichever type of
	 * white balance the hardware implements to either perform one
	 * white balance action or enable auto white balance. */
	VIDEO_CONTROL_WHITE_BALANCE_ACTION,
	VIDEO_CONTROL_WHITE_BALANCE_AUTO,
	VIDEO_CONTROL_WHITE_BALANCE_TEMPERATURE,
	VIDEO_CONTROL_WHITE_BALANCE_TEMPERATURE_AUTO,
	VIDEO_CONTROL_WHITE_BALANCE_COMPONENT,
	VIDEO_CONTROL_WHITE_BALANCE_COMPONENT_AUTO,
	VIDEO_CONTROL_DIGITAL_MULTIPLIER,
	VIDEO_CONTROL_DIGITAL_MULTIPLIER_LIMIT,
	VIDEO_CONTROL_HUE_AUTO,
	VIDEO_CONTROL_ANALOG_VIDEO_STANDARD,
	VIDEO_CONTROL_ANALOG_LOCK_STATUS,
	/* video stream */
	VIDEO_CONTROL_GENERATE_KEY_FRAME,
	VIDEO_CONTROL_UPDATE_FRAME_SEGMENT,
	/* misc, not in UVC */
	VIDEO_CONTROL_HFLIP,
	VIDEO_CONTROL_VFLIP,
	/* Custom controls start here; any controls beyond this are
	 * valid and condsidered "extended". */
	VIDEO_CONTROL_EXTENDED
};

enum video_control_type {
	VIDEO_CONTROL_TYPE_INT, /* signed 32 bit integer */
	VIDEO_CONTROL_TYPE_BOOL,
	VIDEO_CONTROL_TYPE_LIST,  /* V4L2 MENU */
	VIDEO_CONTROL_TYPE_ACTION /* V4L2 BUTTON */
};

#define VIDEO_CONTROL_FLAG_READ		(1<<0)
#define VIDEO_CONTROL_FLAG_WRITE	(1<<1)
#define VIDEO_CONTROL_FLAG_DISABLED	(1<<2) /* V4L2 INACTIVE */
#define VIDEO_CONTROL_FLAG_AUTOUPDATE	(1<<3)
#define VIDEO_CONTROL_FLAG_ASYNC	(1<<4)

struct video_control_desc {
	uint16_t	group_id;
	uint16_t	control_id;
	uint8_t		name[32];
	uint32_t	flags;
	enum video_control_type type;
	int32_t		min;
	int32_t		max;
	int32_t		step;
	int32_t		def;
};

/* array of struct video_control_value_info belonging to the same control */
struct video_control_desc_group {
	uint16_t	group_id;
	uint8_t		length;
	struct video_control_desc *desc;
};

struct video_control {
	uint16_t	group_id;
	uint16_t	control_id;
	int32_t		value;
};

/* array of struct video_control_value belonging to the same control */
struct video_control_group {
	uint16_t	group_id;
	uint8_t		length;
	struct video_control *control;
};

struct video_control_iter {
	struct video_control_desc *desc;
};

/* format of video data in a video sample */
enum video_pixel_format {
	VIDEO_FORMAT_UNDEFINED,
	
	/* uncompressed frame-based formats */
	VIDEO_FORMAT_YUY2,	/* packed 4:2:2 */
	VIDEO_FORMAT_NV12,	/* planar 4:2:0 */
	VIDEO_FORMAT_RGB24,
	VIDEO_FORMAT_RGB555,
	VIDEO_FORMAT_RGB565,
	VIDEO_FORMAT_YUV420,
	VIDEO_FORMAT_SBGGR8,
	VIDEO_FORMAT_UYVY,

	/* compressed frame-based formats */
	VIDEO_FORMAT_MJPEG,	/* frames of JPEG images */
	VIDEO_FORMAT_DV,

	/* stream-based formats */
	VIDEO_FORMAT_MPEG
};

/* interlace_flags bits are allocated like this:
      7 6 5 4 3 2 1 0
	    \_/ | | |interlaced or progressive
	     |  | |packing style of fields (interlaced or planar)
             |  |fields per sample (1 or 2)
             |pattern (F1 only, F2 only, F12, RND)
*/

/* two bits */
#define VIDEO_INTERLACED(iflags) (iflags & 1)
enum video_interlace_presence {
	VIDEO_INTERLACE_OFF = 0, /* progressive */
	VIDEO_INTERLACE_ON = 1,
	VIDEO_INTERLACE_ANY = 2	/* in requests, accept any interlacing */
};

/* one bit, not in UVC */
#define VIDEO_INTERLACE_PACKING(iflags) ((iflags >> 2) & 1)
enum video_interlace_packing {
	VIDEO_INTERLACE_INTERLACED = 0, /* F1 and F2 are interlaced */
	VIDEO_INTERLACE_PLANAR = 1 /* entire F1 is followed by F2 */
};

/* one bit, not in V4L2; Is this not redundant with PATTERN below?
 * For now, I'm assuming it describes where the "end-of-frame" markers
 * appear in the stream data: after every field or after every two
 * fields. */
#define VIDEO_INTERLACE_FIELDS_PER_SAMPLE(iflags) ((iflags >> 3) & 1)
enum video_interlace_fields_per_sample {
	VIDEO_INTERLACE_TWO_FIELDS_PER_SAMPLE = 0,
	VIDEO_INTERLACE_ONE_FIELD_PER_SAMPLE = 1
};

/* two bits */
#define VIDEO_INTERLACE_PATTERN(iflags) ((iflags >> 4) & 3)
enum video_interlace_pattern {
	VIDEO_INTERLACE_PATTERN_F1 = 0,
	VIDEO_INTERLACE_PATTERN_F2 = 1,
	VIDEO_INTERLACE_PATTERN_F12 = 2,
	VIDEO_INTERLACE_PATTERN_RND = 3
};

enum video_color_primaries {
	VIDEO_COLOR_PRIMARIES_UNSPECIFIED,
	VIDEO_COLOR_PRIMARIES_BT709, /* identical to sRGB */
	VIDEO_COLOR_PRIMARIES_BT470_2_M,
	VIDEO_COLOR_PRIMARIES_BT470_2_BG,
	VIDEO_COLOR_PRIMARIES_SMPTE_170M,
	VIDEO_COLOR_PRIMARIES_SMPTE_240M,
	VIDEO_COLOR_PRIMARIES_BT878 /* in V4L2 as broken BT878 chip */
};

enum video_gamma_function {
	VIDEO_GAMMA_FUNCTION_UNSPECIFIED,
	VIDEO_GAMMA_FUNCTION_BT709,
	VIDEO_GAMMA_FUNCTION_BT470_2_M,
	VIDEO_GAMMA_FUNCTION_BT470_2_BG,
	VIDEO_GAMMA_FUNCTION_SMPTE_170M,
	VIDEO_GAMMA_FUNCTION_SMPTE_240M,
	VIDEO_GAMMA_FUNCTION_LINEAR,
	VIDEO_GAMMA_FUNCTION_sRGB, /* similar but not identical to BT709 */
	VIDEO_GAMMA_FUNCTION_BT878 /* in V4L2 as broken BT878 chip */
};

/* Matrix coefficients for converting YUV to RGB */
enum video_matrix_coeff {
	VIDEO_MATRIX_COEFF_UNSPECIFIED,
	VIDEO_MATRIX_COEFF_BT709,
	VIDEO_MATRIX_COEFF_FCC,
	VIDEO_MATRIX_COEFF_BT470_2_BG,
	VIDEO_MATRIX_COEFF_SMPTE_170M,
	VIDEO_MATRIX_COEFF_SMPTE_240M,
	VIDEO_MATRIX_COEFF_BT878 /* in V4L2 as broken BT878 chip */
};

/* UVC spec separates these into three categories.  V4L2 does not. */
struct video_colorspace {
	enum video_color_primaries primaries;
	enum video_gamma_function gamma_function;
	enum video_matrix_coeff matrix_coeff;
};

#ifdef undef
/* Stucts for future split into format/frame/interval.  All functions
 * interacting with the hardware layer will deal with these structs.
 * This video layer will handle translating them to V4L2 structs as
 * necessary. */

struct video_format {
	enum video_pixel_format	vfo_pixel_format;
	uint8_t			vfo_aspect_x; /* aspect ratio x and y */
	uint8_t			vfo_aspect_y;
	struct video_colorspace	vfo_color;
	uint8_t			vfo_interlace_flags;
};

struct video_frame {
	uint32_t	vfr_width; /* dimensions in pixels */
	uint32_t	vfr_height;
	uint32_t	vfr_sample_size; /* max sample size */
	uint32_t	vfr_stride; /* length of one row of pixels in
				     * bytes; uncompressed formats
				     * only */
};

enum video_frame_interval_type {
	VIDEO_FRAME_INTERVAL_TYPE_CONTINUOUS,
	VIDEO_FRAME_INTERVAL_TYPE_DISCRETE
};

/* UVC spec frame interval units are 100s of nanoseconds.  V4L2 spec
 * uses a {32/32} bit struct fraction in seconds. We use 100ns units
 * here. */
#define VIDEO_FRAME_INTERVAL_UNITS_PER_US (10)
#define VIDEO_FRAME_INTERVAL_UNITS_PER_MS (10 * 1000)
#define VIDEO_FRAME_INTERVAL_UNITS_PER_S  (10 * 1000 * 1000)
struct video_frame_interval {
	enum video_frame_interval_type	vfi_type;
	union {
		struct {
			uint32_t min;
			uint32_t max;
			uint32_t step;
		} vfi_continuous;

		uint32_t	vfi_discrete;
	};
};
#endif /* undef */

/* Describes a video format.  For frame based formats, one sample is
 * equivalent to one frame.  For stream based formats such as MPEG, a
 * sample is logical unit of that streaming format.
 */
struct video_format {
	enum video_pixel_format pixel_format;
	uint32_t	width;	/* dimensions in pixels */
	uint32_t	height;
	uint8_t		aspect_x; /* aspect ratio x and y */
	uint8_t		aspect_y;
	uint32_t	sample_size; /* max sample size */
	uint32_t	stride;	     /* length of one row of pixels in
				      * bytes; uncompressed formats
				      * only */
	struct video_colorspace color;
	uint8_t		interlace_flags;
	uint32_t	priv;	/* For private use by hardware driver.
				 * Must be set to zero if not used. */
};

/* A payload is the smallest unit transfered from the hardware driver
 * to the video layer. Multiple video payloads make up one video
 * sample. */
struct video_payload {
	const uint8_t	*data;
	size_t		size;		/* size in bytes of this payload */
	int		frameno;	/* toggles between 0 and 1 */
	bool		end_of_frame;	/* set if this is the last
					 * payload in the frame. */
};

struct video_hw_if {
	int	(*open)(void *, int); /* open hardware */
	void	(*close)(void *);     /* close hardware */

	const char *	(*get_devname)(void *);

	int	(*enum_format)(void *, uint32_t, struct video_format *);
	int	(*get_format)(void *, struct video_format *);
	int	(*set_format)(void *, struct video_format *);
	int	(*try_format)(void *, struct video_format *);

	int	(*start_transfer)(void *);
	int	(*stop_transfer)(void *);

	int	(*control_iter_init)(void *, struct video_control_iter *);
	int	(*control_iter_next)(void *, struct video_control_iter *);
	int	(*get_control_desc_group)(void *,
					  struct video_control_desc_group *);
	int	(*get_control_group)(void *, struct video_control_group *);
	int	(*set_control_group)(void *, const struct video_control_group *);
};

struct video_attach_args {
	const struct video_hw_if *hw_if;
};

device_t video_attach_mi(const struct video_hw_if *, device_t);
void video_submit_payload(device_t, const struct video_payload *);

#endif	/* _SYS_DEV_VIDEO_IF_H_ */

/*	$NetBSD: ossaudiovar.h,v 1.6.8.1 1999/12/27 18:34:31 wrstuden Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
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

struct oss_sys_ioctl_args {
	syscallarg(int) fd;
	syscallarg(u_long) com;
	syscallarg(caddr_t) data;
};

#define OSS_IOCPARM_MASK    0xfff           /* parameters must be < 4096 bytes */
#define OSS_IOC_VOID        0x00000000      /* no parameters */
#define OSS_IOC_IN          0x40000000      /* copy in parameters */
#define OSS_IOC_OUT         0x80000000      /* copy out parameters */
#define OSS_IOC_INOUT       (OSS_IOC_IN | OSS_IOC_OUT)
#define	_OSS_IOCTL(w,x,y,z) ((int)((w)|(((z)&OSS_IOCPARM_MASK)<<16)|((x)<<8)|(y)))
#define _OSS_IO(x,y)        _OSS_IOCTL(OSS_IOC_VOID, x, y, 0)
#define _OSS_IOR(x,y,t)     _OSS_IOCTL(OSS_IOC_OUT, x, y, sizeof(t))
#define _OSS_IOW(x,y,t)     _OSS_IOCTL(OSS_IOC_IN, x, y, sizeof(t))
#define _OSS_IOWR(x,y,t)    _OSS_IOCTL(OSS_IOC_INOUT, x, y, sizeof(t))

#define OSS_IOCTL_SIZE(x) (((x) >> 16) & OSS_IOCPARM_MASK)

#define	OSS_SNDCTL_DSP_RESET		_OSS_IO  ('P', 0)
#define	OSS_SNDCTL_DSP_SYNC		_OSS_IO  ('P', 1)
#define	OSS_SNDCTL_DSP_SPEED		_OSS_IOWR('P', 2, int)
#define	OSS_SOUND_PCM_READ_RATE		_OSS_IOR ('P', 2, int)
#define	OSS_SNDCTL_DSP_STEREO		_OSS_IOWR('P', 3, int)
#define	OSS_SNDCTL_DSP_GETBLKSIZE	_OSS_IOWR('P', 4, int)
#define	OSS_SNDCTL_DSP_SETFMT		_OSS_IOWR('P', 5, int)
#define	OSS_SOUND_PCM_READ_BITS		_OSS_IOR ('P', 5, int)
#define	OSS_SNDCTL_DSP_CHANNELS		_OSS_IOWR('P', 6, int)
#define	OSS_SOUND_PCM_READ_CHANNELS	_OSS_IOR ('P', 6, int)
#define OSS_SOUND_PCM_WRITE_FILTER	_OSS_IOWR('P', 7, int)
#define OSS_SOUND_PCM_READ_FILTER	_OSS_IOR ('P', 7, int)
#define	OSS_SNDCTL_DSP_POST		_OSS_IO  ('P', 8)
#define OSS_SNDCTL_DSP_SUBDIVIDE	_OSS_IOWR('P', 9, int)
#define	OSS_SNDCTL_DSP_SETFRAGMENT	_OSS_IOWR('P', 10, int)
#define	OSS_SNDCTL_DSP_GETFMTS		_OSS_IOR ('P', 11, int)
#define OSS_SNDCTL_DSP_GETOSPACE	_OSS_IOR ('P',12, struct oss_audio_buf_info)
#define OSS_SNDCTL_DSP_GETISPACE	_OSS_IOR ('P',13, struct oss_audio_buf_info)
#define OSS_SNDCTL_DSP_NONBLOCK		_OSS_IO  ('P',14)
#define OSS_SNDCTL_DSP_GETCAPS		_OSS_IOR ('P',15, int)
# define OSS_DSP_CAP_REVISION		0x000000ff
# define OSS_DSP_CAP_DUPLEX		0x00000100
# define OSS_DSP_CAP_REALTIME		0x00000200
# define OSS_DSP_CAP_BATCH		0x00000400
# define OSS_DSP_CAP_COPROC		0x00000800
# define OSS_DSP_CAP_TRIGGER		0x00001000
# define OSS_DSP_CAP_MMAP		0x00002000
#define OSS_SNDCTL_DSP_GETTRIGGER	_OSS_IOR ('P', 16, int)
#define OSS_SNDCTL_DSP_SETTRIGGER	_OSS_IOW ('P', 16, int)
# define OSS_PCM_ENABLE_INPUT		0x00000001
# define OSS_PCM_ENABLE_OUTPUT		0x00000002
#define OSS_SNDCTL_DSP_GETIPTR		_OSS_IOR ('P', 17, struct oss_count_info)
#define OSS_SNDCTL_DSP_GETOPTR		_OSS_IOR ('P', 18, struct oss_count_info)
#define OSS_SNDCTL_DSP_MAPINBUF		_OSS_IOR ('P', 19, struct oss_buffmem_desc)
#define OSS_SNDCTL_DSP_MAPOUTBUF	_OSS_IOR ('P', 20, struct oss_buffmem_desc)
#define OSS_SNDCTL_DSP_SETSYNCRO	_OSS_IO  ('P', 21)
#define OSS_SNDCTL_DSP_SETDUPLEX	_OSS_IO  ('P', 22)
#define OSS_SNDCTL_DSP_PROFILE		_OSS_IOW ('P', 23, int)
#define	  OSS_APF_NORMAL		0	/* Normal applications */
#define	  OSS_APF_NETWORK		1	/* "external" delays */
#define   OSS_APF_CPUINTENS		2	/* CPU delays */

#define	OSS_AFMT_QUERY			0x00000000	/* Return current fmt */
#define	OSS_AFMT_MU_LAW			0x00000001
#define	OSS_AFMT_A_LAW			0x00000002
#define	OSS_AFMT_IMA_ADPCM		0x00000004
#define	OSS_AFMT_U8			0x00000008
#define	OSS_AFMT_S16_LE			0x00000010	/* Little endian signed 16 */
#define	OSS_AFMT_S16_BE			0x00000020	/* Big endian signed 16 */
#define	OSS_AFMT_S8			0x00000040
#define	OSS_AFMT_U16_LE			0x00000080	/* Little endian U16 */
#define	OSS_AFMT_U16_BE			0x00000100	/* Big endian U16 */
#define	OSS_AFMT_MPEG			0x00000200	/* MPEG (2) audio */

/* Mixer defines */
#define OSS_SOUND_MIXER_FIRST		0
#define OSS_SOUND_MIXER_NRDEVICES	17

#define OSS_SOUND_MIXER_VOLUME		0
#define OSS_SOUND_MIXER_BASS		1
#define OSS_SOUND_MIXER_TREBLE		2
#define OSS_SOUND_MIXER_SYNTH		3
#define OSS_SOUND_MIXER_PCM		4
#define OSS_SOUND_MIXER_SPEAKER		5
#define OSS_SOUND_MIXER_LINE		6
#define OSS_SOUND_MIXER_MIC		7
#define OSS_SOUND_MIXER_CD		8
#define OSS_SOUND_MIXER_IMIX		9
#define OSS_SOUND_MIXER_ALTPCM		10
#define OSS_SOUND_MIXER_RECLEV		11
#define OSS_SOUND_MIXER_IGAIN		12
#define OSS_SOUND_MIXER_OGAIN		13
#define OSS_SOUND_MIXER_LINE1		14
#define OSS_SOUND_MIXER_LINE2		15
#define OSS_SOUND_MIXER_LINE3		16

#define OSS_SOUND_MIXER_RECSRC		0xff
#define OSS_SOUND_MIXER_DEVMASK		0xfe
#define OSS_SOUND_MIXER_RECMASK		0xfd
#define OSS_SOUND_MIXER_CAPS		0xfc
#define  OSS_SOUND_CAP_EXCL_INPUT	1
#define OSS_SOUND_MIXER_STEREODEVS	0xfb

#define OSS_MIXER_READ(dev)		_OSS_IOR('M', dev, int)

#define OSS_SOUND_MIXER_READ_RECSRC	OSS_MIXER_READ(OSS_SOUND_MIXER_RECSRC)
#define OSS_SOUND_MIXER_READ_DEVMASK	OSS_MIXER_READ(OSS_SOUND_MIXER_DEVMASK)
#define OSS_SOUND_MIXER_READ_RECMASK	OSS_MIXER_READ(OSS_SOUND_MIXER_RECMASK)
#define OSS_SOUND_MIXER_READ_STEREODEVS	OSS_MIXER_READ(OSS_SOUND_MIXER_STEREODEVS)
#define OSS_SOUND_MIXER_READ_CAPS	OSS_MIXER_READ(OSS_SOUND_MIXER_CAPS)

#define OSS_MIXER_WRITE(dev)		_OSS_IOW('M', dev, int)
#define OSS_MIXER_WRITE_R(dev)		_OSS_IOWR('M', dev, int)

#define OSS_SOUND_MIXER_WRITE_RECSRC	OSS_MIXER_WRITE(OSS_SOUND_MIXER_RECSRC)
#define OSS_SOUND_MIXER_WRITE_R_RECSRC	OSS_MIXER_WRITE_R(OSS_SOUND_MIXER_RECSRC)

struct oss_mixer_info {
	char id[16];
	char name[32];
	int  modify_counter;
	int  fillers[10];
};

struct oss_old_mixer_info {
	char id[16];
	char name[32];
};

#define OSS_SOUND_MIXER_INFO		_OSS_IOR('M', 101, struct oss_mixer_info)
#define OSS_SOUND_OLD_MIXER_INFO	_OSS_IOR('M', 101, struct oss_old_mixer_info)

#define OSS_GET_DEV(com) ((com) & 0xff)

struct oss_audio_buf_info {
	int fragments;
	int fragstotal;
	int fragsize;
	int bytes;
};

struct oss_count_info {
	int bytes;
	int blocks;
	int ptr;
};

struct oss_buffmem_desc {
	unsigned int *buffer;
	int size;
};

/*
 * MIDI and sequencer I/O.
 */
#define OSS_SEQ_RESET			_OSS_IO  ('Q', 0)
#define OSS_SEQ_SYNC			_OSS_IO  ('Q', 1)
#define OSS_SYNTH_INFO			_OSS_IOWR('Q', 2, struct oss_synth_info)
#define OSS_SEQ_CTRLRATE		_OSS_IOWR('Q', 3, int)
#define OSS_SEQ_GETOUTCOUNT		_OSS_IOR ('Q', 4, int)
#define OSS_SEQ_GETINCOUNT		_OSS_IOR ('Q', 5, int)
#define OSS_SEQ_PERCMODE		_OSS_IOW ('Q', 6, int)
#define OSS_SEQ_TESTMIDI		_OSS_IOW ('Q', 8, int)
#define OSS_SEQ_RESETSAMPLES		_OSS_IOW ('Q', 9, int)
#define OSS_SEQ_NRSYNTHS		_OSS_IOR ('Q',10, int)
#define OSS_SEQ_NRMIDIS			_OSS_IOR ('Q',11, int)
#define OSS_MIDI_INFO			_OSS_IOWR('Q',12, struct oss_midi_info)
#define OSS_SEQ_THRESHOLD		_OSS_IOW ('Q',13, int)
#define OSS_MEMAVL			_OSS_IOWR('Q',14, int)
#define OSS_FM_4OP_ENABLE		_OSS_IOW ('Q',15, int)
#define OSS_SEQ_PANIC			_OSS_IO  ('Q',17)
#define OSS_SEQ_OUTOFBAND		_OSS_IOW ('Q',18, struct oss_seq_event_rec)
#define OSS_SEQ_GETTIME			_OSS_IOR ('Q',19, int)
#define OSS_ID				_OSS_IOWR('Q',20, struct oss_synth_info)
#define OSS_CONTROL			_OSS_IOWR('Q',21, struct oss_synth_control)
#define OSS_REMOVESAMPLE		_OSS_IOWR('Q',22, struct oss_remove_sample)

struct oss_synth_control {
	int devno;	/* Synthesizer # */
	char data[4000]; /* Device specific command/data record */
};

struct oss_remove_sample {
	int devno;	/* Synthesizer # */
	int bankno;	/* MIDI bank # (0=General MIDI) */
	int instrno;	/* MIDI instrument number */
};

struct oss_seq_event_rec {
	u_char arr[8];
};

struct oss_synth_info {
	char	name[30];
	int	device;
	int	synth_type;
#define OSS_SYNTH_TYPE_FM		0
#define OSS_SYNTH_TYPE_SAMPLE		1
#define OSS_SYNTH_TYPE_MIDI		2

	int	synth_subtype;
#define OSS_FM_TYPE_ADLIB		0x00
#define OSS_FM_TYPE_OPL3		0x01
#define OSS_MIDI_TYPE_MPU401		0x401

#define OSS_SAMPLE_TYPE_BASIC		0x10
#define OSS_SAMPLE_TYPE_GUS		OSS_SAMPLE_TYPE_BASIC

	int	perc_mode;
	int	nr_voices;
	int	nr_drums;
	int	instr_bank_size;
	u_int	capabilities;	
#define OSS_SYNTH_CAP_PERCMODE		0x00000001
#define OSS_SYNTH_CAP_OPL3		0x00000002
#define OSS_SYNTH_CAP_INPUT		0x00000004
	int	_unused[19];
};

#define OSS_TMR_TIMEBASE		_OSS_IOWR('T', 1, int)
#define OSS_TMR_START			_OSS_IO  ('T', 2)
#define OSS_TMR_STOP			_OSS_IO  ('T', 3)
#define OSS_TMR_CONTINUE		_OSS_IO  ('T', 4)
#define OSS_TMR_TEMPO			_OSS_IOWR('T', 5, int)
#define OSS_TMR_SOURCE			_OSS_IOWR('T', 6, int)
#  define OSS_TMR_INTERNAL		0x00000001
#  define OSS_TMR_EXTERNAL		0x00000002
#  define OSS_TMR_MODE_MIDI		0x00000010
#  define OSS_TMR_MODE_FSK		0x00000020
#  define OSS_TMR_MODE_CLS		0x00000040
#  define OSS_TMR_MODE_SMPTE		0x00000080
#define OSS_TMR_METRONOME		_OSS_IOW ('T', 7, int)
#define OSS_TMR_SELECT			_OSS_IOW ('T', 8, int)


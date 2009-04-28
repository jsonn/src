/*	$NetBSD: disptest.c,v 1.5.134.1 2009/04/28 07:34:05 skrll Exp $	*/

/*-
 * Copyright (c) 1999 Shin Takemura.
 * All rights reserved.
 *
 * This software is part of the PocketBSD.
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
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
#include <pbsdboot.h>

extern BOOL SetKMode(BOOL);
#define ARRAYSIZEOF(a)	(sizeof(a)/sizeof(*(a)))

static struct area {
  long start, end;
} targets[] = {
	{ 0x0a000000, 0x0b000000 },
	{ 0x10000000, 0x14000000 },
};
int ntargets = ARRAYSIZEOF(targets);

void
flush_XX(void)
{
  static volatile unsigned char tmp[1024*64];
  int i, s;

  for (i = 0; i < ARRAYSIZEOF(tmp); i++) {
	  s += tmp[i];
  }
}

static void
gpio_test(void)
{
#define GIUBASE 0xab000000
#define GIUOFFSET 0x0100
	volatile unsigned short *giusell;
	volatile unsigned short *giuselh;
	volatile unsigned short *giupiodl;
	volatile unsigned short *giupiodh;
	unsigned short sell = 0;
	unsigned short selh = 0;
	unsigned short piodl = 0;
	unsigned short piodh = 0;
	int res, i;
	unsigned short regs[16];
	unsigned short prev_regs[16];

	unsigned char* p = (char*)VirtualAlloc(0, 1024, MEM_RESERVE,
				      PAGE_NOACCESS);
	res = VirtualCopy((LPVOID)p, (LPVOID)(GIUBASE >> 8), 1024,
			  PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL);
	if (!res) {
		win_printf(TEXT("VirtualCopy() failed."));
	}

	for (i = 0; i < 16; i++) {
		prev_regs[i] = ~0;
	}

	giusell = (unsigned short*)(p + GIUOFFSET + 0);
	giuselh = (unsigned short*)(p + GIUOFFSET + 2);
	giupiodl = (unsigned short*)(p + GIUOFFSET + 4);
	giupiodh = (unsigned short*)(p + GIUOFFSET + 6);

	while (1) {
		sell = *giusell;
		selh = *giuselh;
		*giusell = sell;
		*giuselh = selh;

		piodl = *giupiodl;
		piodh = *giupiodh;
		*giupiodl = piodl;
		*giupiodh = piodh;
		for (i = 0; i < 16; i++) {
			regs[i] = *(unsigned short*)(p + GIUOFFSET + i * 2);
		}
		for (i = 0; i < 16; i++) {
			if (i != 3 && i != 5 && regs[i] != prev_regs[i]) {
				win_printf(TEXT("IOSEL=%04x%04x "),
					   regs[1], regs[0]);
				win_printf(TEXT("PIOD=%04x%04x "),
					   regs[3], regs[2]);
				win_printf(TEXT("STAT=%04x%04x "),
					   regs[5], regs[4]);
				win_printf(TEXT("(%04x%04x) "),
					   regs[5]&regs[7], regs[4]&regs[6]);
				win_printf(TEXT("EN=%04x%04x "),
					   regs[7], regs[6]);
				win_printf(TEXT("TYP=%04x%04x "),
					   regs[9], regs[8]);
				win_printf(TEXT("ALSEL=%04x%04x "),
					   regs[11], regs[10]);
				win_printf(TEXT("HTSEL=%04x%04x "),
					   regs[13], regs[12]);
				win_printf(TEXT("PODAT=%04x%04x "),
					   regs[15], regs[14]);
				win_printf(TEXT("\n"));
				for (i = 0; i < 16; i++) {
					prev_regs[i] = regs[i];
				}
				break;
			}
		}
	}
}

static struct regdesc {
	TCHAR *name;
	int physaddr;
	int size;
	int mask;
	//void *addr;
	unsigned long val;
	unsigned long preval;
} test_regs[] = {
#if 0
	/*
	 * Vrc4172 GPIO and PWM
	 */
	{ TEXT("EXGPDATA0"),	0x15001080,	2,	0xfffd		},
	{ TEXT("EXGPDATA1"),	0x150010c0,	2,	0xffff		},
	{ TEXT("LCDDUTYEN"),	0x15003880,	2,	0xffff		},
	{ TEXT("LCDFREQ"),	0x15003882,	2,	0xffff		},
	{ TEXT("LCDDUTY"),	0x15003884,	2,	0xffff		},
#endif

#if 0
	/*
	 * Vr41xx GPIO
	 */
	{ TEXT("GIUPIODL"),	0x0b000104,	2,	0xffff		},
	{ TEXT("GIUPIODH"),	0x0b000106,	2,	0xffff		},
	{ TEXT("GIUPODATL"),	0x0b00011c,	2,	0xffff		},
	{ TEXT("GIUPODATH"),	0x0b00011e,	2,	0xffff		},
	{ TEXT("GIUUSEUPDN"),	0x0b0002e0,	2,	0xffff		},
	{ TEXT("GIUTERMUPDN"),	0x0b0002e2,	2,	0xffff		},
#endif

	/*
	 * MQ200
	 */
	{ TEXT("PM00R"),	0x0a600000,	4,	0xffffffff	},
	{ TEXT("PM01R"),	0x0a600004,	4,	0xffffffff	},
	{ TEXT("PM02R"),	0x0a600008,	4,	0xffffffff	},
	{ TEXT("PM06R"),	0x0a600018,	4,	0xffffffff	},
	{ TEXT("PM07R"),	0x0a60001c,	4,	0xffffffff	},

	{ TEXT("CC00R"),	0x0a602000,	4,	0x0000003f	},
	{ TEXT("CC01R"),	0x0a602004,	4,	0x00000000	},

	{ TEXT("MM00R"),	0x0a604000,	4,	0x00000007	},
	{ TEXT("MM01R"),	0x0a604004,	4,	0xffffffff	},
	{ TEXT("MM02R"),	0x0a604008,	4,	0xffffffff	},
	{ TEXT("MM03R"),	0x0a60400c,	4,	0x00000001	},
	{ TEXT("MM04R"),	0x0a604010,	4,	0x00000001	},

	{ TEXT("IN00R"),	0x0a608000,	4,	0x0000001f	},
	{ TEXT("IN01R"),	0x0a608004,	4,	0x0000ffff	},
	{ TEXT("IN02R"),	0x0a608008,	4,	0x00000000	},
	{ TEXT("IN03R"),	0x0a60800c,	4,	0x00000000	},

	{ TEXT("GC00R"),	0x0a60a000,	4,	0xfffff9ff	},
	{ TEXT("GC01R"),	0x0a60a004,	4,	0x10ffffff	},
	{ TEXT("GC20R"),	0x0a60a080,	4,	0xffffffff	},
	{ TEXT("GC21R"),	0x0a60a084,	4,	0x0000007f	},

	{ TEXT("FP00R"),	0x0a60e000,	4,	0xffffffff	},
	{ TEXT("FP01R"),	0x0a60e004,	4,	0xffffffff	},
	{ TEXT("FP02R"),	0x0a60e008,	4,	0x007fffff	},
	{ TEXT("FP03R"),	0x0a60e00c,	4,	0x0707003f	},
	{ TEXT("FP04R"),	0x0a60e010,	4,	0xffff3fff	},
	{ TEXT("FP05R"),	0x0a60e014,	4,	0xffffffff	},
	{ TEXT("FP0FR"),	0x0a60e03c,	4,	0xffffffff	},

	{ TEXT("DC00R"),	0x0a614000,	4,	0xffffffff	},
	{ TEXT("DC01R"),	0x0a614004,	4,	0x0000003f	},
	{ TEXT("DC02R"),	0x0a614008,	4,	0xffffffff	},
	{ TEXT("DC03R"),	0x0a61400c,	4,	0xffffffff	},

	{ TEXT("PC00R"),	0x0a616000,	4,	0xffffffff	},
	{ TEXT("PC04R"),	0x0a616004,	4,	0xffffffff	},
	{ TEXT("PC08R"),	0x0a616008,	4,	0xffffffff	},
	{ TEXT("PC0CR"),	0x0a61600c,	4,	0xffffffff	},
	{ TEXT("PC10R"),	0x0a616010,	4,	0xffffffff	},
	{ TEXT("PC14R"),	0x0a616014,	4,	0xffffffff	},
	{ TEXT("PC2CR"),	0x0a61602c,	4,	0xffffffff	},
	{ TEXT("PC3CR"),	0x0a61603c,	4,	0xffffffff	},
	{ TEXT("PC40R"),	0x0a616040,	4,	0xffffffff	},
	{ TEXT("PC44R"),	0x0a616044,	4,	0x00000003	},
};

extern int SetKMode(int);
static void
regfetch(struct regdesc* desc)
{
	SetKMode(1);
	switch (desc->size) {
	case 1:
		desc->val = *(unsigned char*)(desc->physaddr | 0xa0000000);
		break;
	case 2:
		desc->val = *(unsigned short*)(desc->physaddr | 0xa0000000);
		break;
	case 4:
		desc->val = *(unsigned long*)(desc->physaddr | 0xa0000000);
		break;
	default:
		win_printf(TEXT("Invalid size"));
		break;
	}
	SetKMode(0);
	desc->val &= desc->mask;
}

static void
register_test(void)
{
    int i;
    int nregs = sizeof(test_regs)/sizeof(*test_regs);

    for (i = 0; i < nregs; i++) {
	regfetch(&test_regs[i]);
	test_regs[i].preval = test_regs[i].val;
    }

    while (1) {
	for (i = 0; i < nregs; i++) {
	    regfetch(&test_regs[i]);
	    if (test_regs[i].val != test_regs[i].preval) {
		win_printf(TEXT("%20s(%08x) %08x -> %08x\n"),
		    test_regs[i].name,
		    test_regs[i].physaddr,
		    test_regs[i].preval,
		    test_regs[i].val);
		test_regs[i].preval = test_regs[i].val;
	    }
	}
	Sleep(10); /* 10 msec */
    }
}

static void
dump_memory(void)
{
	HANDLE fh = INVALID_HANDLE_VALUE;
#define UNICODE_MEMORY_CARD \
	TEXT('\\'), 0xff92, 0xff93, 0xff98, TEXT(' '), 0xff76, 0xff70, \
	0xff84, 0xff9e
	TCHAR filename[] = { UNICODE_MEMORY_CARD,  TEXT('2'), TEXT('\\'),
			     TEXT('d'), TEXT('u'),  TEXT('m'), TEXT('p'), 0 };
	unsigned long *addr;
	int found;

	win_printf(TEXT("dump to %s\n"), filename);
	fh = CreateFile(
		filename,      	/* file name */
		GENERIC_WRITE,	/* access (read-write) mode */
		FILE_SHARE_WRITE,/* share mode */
		NULL,		/* pointer to security attributes */
		CREATE_ALWAYS,	/* how to create */
		FILE_ATTRIBUTE_NORMAL,	/* file attributes*/
		NULL		/* handle to file with attributes to */
	    );
	if (fh == INVALID_HANDLE_VALUE) {
		return;
	}

	for (addr = (unsigned long*)0xbe000000;
	    addr < (unsigned long*)0xbfffffff;
	    addr += 2048) {
		char buf[2048];
		DWORD n;

		SetKMode(1);
		memcpy(buf, addr, 2048);
		SetKMode(0);
		if (WriteFile(fh, buf, 2048, &n, NULL) == 0 ||
		    n != 2048) {
			win_printf(TEXT("dump failed\n"));
			break;
		}
	}

	CloseHandle(fh);
}

static void
serial_test(void)
{
#if 1
#  define SIUADDR 0xac000000
#  define REGOFFSET 0x0
#else
#  define SIUADDR 0xab000000
#  define REGOFFSET 0x1a0
#endif
#define REGSIZE 32
	int i, changed, res;
	unsigned char regs[REGSIZE], prev_regs[REGSIZE];
	unsigned char* p = (char*)VirtualAlloc(0, 1024, MEM_RESERVE,
				      PAGE_NOACCESS);
	
	for (i = 0; i < ARRAYSIZEOF(prev_regs); i++) {
		prev_regs[i] = ~0;
	}

	res = VirtualCopy((LPVOID)p, (LPVOID)(SIUADDR >> 8), 1024,
			  PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL);
	if (!res) {
		win_printf(TEXT("VirtualCopy() failed."));
	}

	while (1) {
		flush_XX();

		for (i = 0; i < ARRAYSIZEOF(regs); i++) {
			regs[i] = p[REGOFFSET + i];
		}

		changed = 0;
		for (i = 0; i < ARRAYSIZEOF(regs); i++) {
			if (regs[i] != prev_regs[i]) {
				changed++;
			}
			prev_regs[i] = regs[i];
		}
		if (changed) {
			win_printf(TEXT("SIU regs: "));
			for (i = 0; i < ARRAYSIZEOF(regs); i++) {
				win_printf(TEXT("%02x "), regs[i]);
			}
			win_printf(TEXT("\n"));
		}
	}

	VirtualFree(p, 0, MEM_RELEASE);
}

static long
checksum(char* addr, int size)
{
	long sum = 0;
	int i;

	for (i = 0; i < size; i++) {
		sum += *addr++ * i;
	}
	return (sum);
}

static int
examine(char* addr, int size)
{
	long random_data[256];
	long dijest;
	int i;

	for (i = 0; i < ARRAYSIZEOF(random_data); i++) {
		random_data[i] = Random();
	}
	if (sizeof(random_data) < size) {
		size = sizeof(random_data);
	}
	memcpy(addr, (char*)random_data, size);
	dijest= checksum((char*)random_data, size);

	return (dijest == checksum(addr, size));
}

void
display_search(void)
{
	int step = 0x10000;
	int i;
	long addr;

	for (i = 0; i < ntargets; i++) {
		int prevres = -1;
		for (addr = targets[i].start;
		     addr < targets[i].end;
		     addr += step) {
			int res;
#if 0
			char* p = (char*)VirtualAlloc(0, step, MEM_RESERVE,
						      PAGE_NOACCESS);
			res = VirtualCopy((LPVOID)p, (LPVOID)(addr >> 8), step,
				   PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL);
			if (!res) {
				win_printf(TEXT("VirtualCopy() failed."));
			}
			res = examine(p, step);
			VirtualFree(p, 0, MEM_RELEASE);
#else
			SetKMode(1);
			res = examine((char*)((int)addr | 0xa0000000), step);
			SetKMode(0);
#endif
			if (res != prevres && prevres != -1) {
				if (res) {
					win_printf(TEXT("0x%x "), addr);
				} else {
					win_printf(TEXT("- 0x%x\n"), addr);
				}
			} else
			if (res && prevres == -1) {
				win_printf(TEXT("0x%x "), addr);
			}
			prevres = res;
		}
		if (prevres) {
			win_printf(TEXT("\n"));
		}
	}
}

void
display_draw(void)
{
	long addr = 0x13000000;
	int size = 0x80000;
	char* p;
	int i, j, res;
	int x, y;
	int stride = 1280;

	p = (char*)VirtualAlloc(0, size, MEM_RESERVE,
				PAGE_NOACCESS);
	res = VirtualCopy((LPVOID)p, (LPVOID)(addr >> 8), size,
			  PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL);
	if (!res) {
		win_printf(TEXT("VirtualCopy() failed."));
	}

	for (i = 0; i < 10000; i++) {
		p[i] = i;
	}
	for (x = 0; x < 640; x += 10) {
		for (y = 0; y < 240; y += 1) {
		        p[stride * y + x] = (char)0xff;
		}
	}
	for (y = 0; y < 240; y += 10) {
		for (x = 0; x < 640; x += 1) {
		        p[stride * y + x] = (char)0xff;
		}
	}
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 16; j++) {
			for (x = i * 32; x < i * 32 + 32; x++) {
				for (y = j * 15; y < j * 15 + 15; y++) {
					p[stride * y + x] = j * 16 + i;
				}
			}
		}
	}

	VirtualFree(p, 0, MEM_RELEASE);
}

#define PCIC_IDENT		0x00
#define	PCIC_REG_INDEX		0
#define	PCIC_REG_DATA		1
#define PCIC_IDENT_EXPECTED	0x83

void
pcic_search(void)
{
	long addr;
	int window_size = 0x10000;
	int i;

	for (addr = 0x14000000; addr < 0x18000000; addr += window_size) {
		int res;
		unsigned char* p;
		p = (char*)VirtualAlloc(0, window_size, MEM_RESERVE,
					PAGE_NOACCESS);
		res = VirtualCopy((LPVOID)p, (LPVOID)(addr >> 8), window_size,
				  PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL);
		if (!res) {
			win_printf(TEXT("VirtualCopy() failed."));
		}

		for (i = 0; i < window_size; i += 2) {
			p[i + PCIC_REG_INDEX] = PCIC_IDENT;
			if (p[i + PCIC_REG_DATA] == PCIC_IDENT_EXPECTED) {
				win_printf(TEXT("pcic is found at 0x%x\n"),
					   addr + i);
			}
		}

		VirtualFree(p, 0, MEM_RELEASE);
	}
}

#define VRPCIU_CONFA	(*(u_int32_t*)0xaf000c18)
#define VRPCIU_CONFD	(*(u_int32_t*)0xaf000c14)

void
pci_dump(void)
{
	int mode, i;
	BOOL SetKMode(BOOL);
	int bus, dev;
	u_int32_t addr, val;
	u_int32_t addrs[] = {
		0x00000800,
		0x00001000,
		0x00002000,
		0x00004000,
		0x00008000,
		0x00010000,
		0x00020000,
		0x00040000,
		0x00080000,
		0x00100000,
		0x00200000,
		0x00400000,
		0x00800000,
		0x01000000,
		0x02000000,
		0x04000000,
		0x08000000,
		0x10000000,
		0x20000000,
		0x40000000,
		0x80000000,
	};

#if 0 /* You can find Vrc4173 BCU at 0xb6010000 on Sigmarion II */
	win_printf(TEXT("Vrc4173 CMUCLKMSK:	%04X\n"),
	    *(u_int16_t*)0xb6010040);
	win_printf(TEXT("Vrc4173 CMUSRST:	%04X\n"),
	    *(u_int16_t*)0xb6010042);

	/* enable CARDU clock */
	*(u_int16_t*)0xb6010042 = 0x0006; /* enable CARD1RST and CARD2RST */
	*(u_int16_t*)0xb6010040 = *(u_int16_t*)0xb6010040 | 0x00c0;
	*(u_int16_t*)0xb6010042 = 0x0000; /* disable CARD1RST and CARD2RST */

	win_printf(TEXT("Vrc4173 CMUCLKMSK:	%04X\n"),
	    *(u_int16_t*)0xb6010040);
	win_printf(TEXT("Vrc4173 CMUSRST:	%04X\n"),
	    *(u_int16_t*)0xb6010042);
#endif

	for (i = 0; i < sizeof(addrs)/sizeof(*addrs); i++) {
		VRPCIU_CONFA = addrs[i];
		val = VRPCIU_CONFD;
		win_printf(TEXT("%2d:	%08X	%04X %04X\n"),
		    i, addrs[i], val & 0xffff, (val >> 16) & 0xffff);
	}

	mode = SetKMode(1);
	SetKMode(mode);
}

void
hardware_test(void)
{
	int do_gpio_test = 0;
	int do_register_test = 0;
	int do_serial_test = 0;
	int do_display_draw = 0;
	int do_display_search = 0;
	int do_pcic_search = 0;
	int do_dump_memory = 0;
	int do_pci_dump = 0;

	if (do_gpio_test) {
		gpio_test();
	}
	if (do_register_test) {
		register_test();
	}
	if (do_serial_test) {
		serial_test();
	}
	if (do_display_draw) {
		display_draw();
	}
	if (do_display_search) {
		display_search();
	}
	if (do_pcic_search) {
		pcic_search();
	}
	if (do_dump_memory) {
		dump_memory();
	}
	if (do_pci_dump) {
		pci_dump();
	}
}

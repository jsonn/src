/*	$NetBSD: conf.h,v 1.1.10.1 1997/10/15 05:34:29 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * conf.h
 *
 * Prototypes for device driver functions
 */
 
#include <sys/conf.h>

bdev_decl(wd);
cdev_decl(wd);
bdev_decl(sw);
cdev_decl(sw);
bdev_decl(fd);
cdev_decl(fd);
bdev_decl(md);
cdev_decl(md);

/* Character device declarations */

/* open, close, read, write, ioctl, tty, mmap */
#define cdev_physcon_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	dev_init(c,n,tty), ttpoll, dev_init(c,n,mmap), 0 }

/* open, close, write, ioctl */
#define cdev_lpt_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev, 0 }

/* open, close, write, ioctl */
#define cdev_beep_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev, 0 }

/* open, close, write, ioctl */
#define cdev_kbd_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev, 0 }

/* open, close, write, ioctl */
#define cdev_cpu_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev, 0 }

/* open, close, write, ioctl */
#define cdev_vidcvid_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, \
	0, seltrue, dev_init(c,n,mmap), 0 }

/* open, close, write, ioctl */
#define cdev_iic_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev, 0 }

/* open, close, write, ioctl */
#define cdev_rtc_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev, 0 }

#define mmread  mmrw
#define mmwrite mmrw
cdev_decl(mm);

cdev_decl(physcon);
cdev_decl(vidcvideo);
cdev_decl(com);
cdev_decl(lpt);
cdev_decl(qms);
cdev_decl(pms);
cdev_decl(beep);
cdev_decl(kbd);
cdev_decl(cpu);
cdev_decl(iic);
cdev_decl(rtc);

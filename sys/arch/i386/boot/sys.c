/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	$Id: sys.c,v 1.6.2.2 1994/02/03 22:56:34 mycroft Exp $
 */

#include "boot.h"
#include <sys/dirent.h>
#include <sys/reboot.h>

char mapbuf[MAXBSIZE], iobuf[MAXBSIZE], fsbuf[SBSIZE];
int mapblock = 0;

void bcopy(), pcpy();

read(buffer, count)
	char *buffer;
	int count;
{
	_read(buffer, count, bcopy);
}

xread(buffer, count)
	char *buffer;
	int count;
{
	_read(buffer, count, pcpy);
}

_read(buffer, count, copy)
	char *buffer;
	int count;
	void (*copy)();
{
	int logno, off, size;
	int cnt2;

	while (count) {
		off = blkoff(fs, poff);
		logno = lblkno(fs, poff);
		cnt2 = size = blksize(fs, &inode, logno);
		bnum = fsbtodb(fs, block_map(logno)) + boff;
		cnt = cnt2;
		iodest = iobuf;
		devread();
		size -= off;
		if (size > count)
			size = count;
		copy(iodest + off, buffer, size);
		buffer += size;
		count -= size;
		poff += size;
	}
}

find(path)
	char *path;
{
	char *rest, ch;
	int block, off, loc, ino = ROOTINO;
	struct dirent *dp;

loop:
	iodest = iobuf;
	cnt = fs->fs_bsize;
	bnum = fsbtodb(fs, itod(fs,ino)) + boff;
	devread();
	bcopy(&((struct dinode *)iodest)[ino % fs->fs_inopb],
	      &inode.i_din,
	      sizeof(struct dinode));
	if (!*path)
		return 1;
	while (*path == '/')
		path++;
	if (!inode.i_size || ((inode.i_mode & IFMT) != IFDIR))
		return 0;
	for (rest = path; (ch = *rest) && ch != '/'; rest++);
	*rest = 0;
	loc = 0;
	do {
		if (loc >= inode.i_size)
			return 0;
		if (!(off = blkoff(fs, loc))) {
			int cnt2;
			block = lblkno(fs, loc);
			cnt2 = blksize(fs, &inode, block);
			bnum = fsbtodb(fs, block_map(block)) + boff;
			cnt = cnt2;
			iodest = iobuf;
			devread();
		}
		dp = (struct dirent *)(iodest + off);
		loc += dp->d_reclen;
	} while (!dp->d_fileno || strcmp(path, dp->d_name));
	ino = dp->d_fileno;
	*(path = rest) = ch;
	goto loop;
}

block_map(file_block)
	int file_block;
{
	if (file_block < NDADDR)
		return(inode.i_db[file_block]);
	if ((bnum = fsbtodb(fs, inode.i_ib[0]) + boff) != mapblock) {
		iodest = mapbuf;
		cnt = fs->fs_bsize;
		devread();
		mapblock = bnum;
	}
	return (((int *)mapbuf)[(file_block - NDADDR) % NINDIR(fs)]);
}

openrd()
{
	char **devp, *cp = name;
	/*******************************************************\
	* If bracket given look for preceding device name	*
	\*******************************************************/
	while (*cp && *cp!='(')
		cp++;
	if (!*cp) {
		cp = name;
	} else {
		if (cp++ != name) {
			for (devp = devs; *devp; devp++)
				if (name[0] == (*devp)[0] &&
				    name[1] == (*devp)[1])
					break;
			if (!*devp) {
				printf("Unknown device\n");
				return 1;
			}
			maj = devp-devs;
		}
		/*******************************************************\
		* Look inside brackets for unit number, and partition	*
		\*******************************************************/
		if (*cp >= '0' && *cp <= '9')
			if ((unit = *cp++ - '0') > 1) {
				printf("Bad unit\n");
				return 1;
			}
		if (!*cp || (*cp == ',' && !*++cp))
			return 1;
		if (*cp >= 'a' && *cp <= 'p')
			part = *cp++ - 'a';
		while (*cp && *cp++!=')') ;
		if (!*cp)
			return 1;
	}
	switch(maj) {
	case 1:
		dosdev = unit | 0x80;
		unit = 0;
		break;
	case 0:
	case 4:
		dosdev = unit | 0x80;
		break;
	case 2:
		dosdev = unit;
		break;
	case 3:
		printf("Wangtek unsupported\n");
		return 1;
		break;
	}
	inode.i_dev = dosdev;
	/***********************************************\
	* Now we know the disk unit and part,		*
	* Load disk info, (open the device)		*
	\***********************************************/
	if (devopen())
		return 1;

	/***********************************************\
	* Load Filesystem info (mount the device)	*
	\***********************************************/
	iodest = (char *)(fs = (struct fs *)fsbuf);
	cnt = SBSIZE;
	bnum = SBLOCK + boff;
	devread();

	/***********************************************\
	* Find the actual FILE on the mounted device	*
	\***********************************************/
	if (!find(cp))
		return 1;

	poff = 0;
	name = cp;
	return 0;
}

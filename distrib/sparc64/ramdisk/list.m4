#	$NetBSD: list.m4,v 1.2.2.4 2000/11/16 17:25:24 tv Exp $

# copy the crunched binary, link to it, and kill it
COPY	${OBJDIR}/ramdiskbin		ramdiskbin
LINK	ramdiskbin			sysinst
LINK	ramdiskbin			bin/cat
LINK	ramdiskbin			bin/chmod
LINK	ramdiskbin			bin/cp
LINK	ramdiskbin			bin/dd
LINK	ramdiskbin			bin/df
LINK	ramdiskbin			bin/ed
LINK	ramdiskbin			bin/ln
LINK	ramdiskbin			bin/ls
LINK	ramdiskbin			bin/mkdir
LINK	ramdiskbin			bin/mt
LINK	ramdiskbin			bin/mv
LINK	ramdiskbin			bin/pax
LINK	ramdiskbin			bin/pwd
LINK	ramdiskbin			bin/rm
LINK	ramdiskbin			bin/sh
LINK	ramdiskbin			bin/stty
LINK	ramdiskbin			bin/sync
LINK	ramdiskbin			bin/test
LINK	ramdiskbin			bin/[
LINK	ramdiskbin			sbin/cd9660
LINK	ramdiskbin			sbin/disklabel
LINK	ramdiskbin			sbin/ffs
LINK	ramdiskbin			sbin/fsck
LINK	ramdiskbin			sbin/fsck_ffs
LINK	ramdiskbin			sbin/halt
LINK	ramdiskbin			sbin/ifconfig
LINK	ramdiskbin			sbin/init
LINK	ramdiskbin			sbin/kernfs
LINK	ramdiskbin			sbin/mknod
LINK	ramdiskbin			sbin/mount
LINK	ramdiskbin			sbin/mount_cd9660
LINK	ramdiskbin			sbin/mount_ext2fs
LINK	ramdiskbin			sbin/mount_ffs
LINK	ramdiskbin			sbin/mount_kernfs
LINK	ramdiskbin			sbin/mount_msdos
LINK	ramdiskbin			sbin/mount_nfs
LINK	ramdiskbin			sbin/msdos
LINK	ramdiskbin			sbin/newfs
LINK	ramdiskbin			sbin/nfs
LINK	ramdiskbin			sbin/ping
LINK	ramdiskbin			sbin/reboot
LINK	ramdiskbin			sbin/restore
LINK	ramdiskbin			sbin/route
LINK	ramdiskbin			sbin/rrestore
LINK	ramdiskbin			sbin/shutdown
LINK	ramdiskbin			sbin/slattach
LINK	ramdiskbin			sbin/swapctl
LINK	ramdiskbin			sbin/umount
ifelse(MACHINE,i386,	LINK	ramdiskbin	sbin/fdisk)
ifelse(MACHINE,i386,	LINK	ramdiskbin	sbin/mbrlabel)
LINK	ramdiskbin		usr/bin/chgrp
LINK	ramdiskbin		usr/bin/ftp
LINK	ramdiskbin		usr/bin/gunzip
LINK	ramdiskbin		usr/bin/gzcat
LINK	ramdiskbin		usr/bin/gzip
LINK	ramdiskbin		usr/bin/less
LINK	ramdiskbin		usr/bin/more
LINK	ramdiskbin		usr/bin/sed
LINK	ramdiskbin		usr/bin/tar
LINK	ramdiskbin		usr/bin/tip
LINK	ramdiskbin		usr/mdec/installboot
LINK	ramdiskbin		usr/sbin/chown
LINK	ramdiskbin		usr/sbin/chroot
ifelse(MACHINE,i386,	LINK	ramdiskbin	usr/sbin/bad144)
ifelse(MACHINE,sparc64,	LINK	ramdiskbin	usr/sbin/chat)
ifelse(MACHINE,sparc64,	LINK	ramdiskbin	usr/sbin/pppd)
ifelse(MACHINE,sparc64,	LINK	ramdiskbin	usr/bin/getopt)
ifelse(MACHINE,sparc64,	LINK	ramdiskbin	sbin/sysctl)
SPECIAL	/bin/rm ramdiskbin

# various files that we need in /etc for the install
COPY	SRCROOT/etc/group		etc/group
COPY	SRCROOT/etc/master.passwd	etc/master.passwd
COPY	SRCROOT/etc/protocols	etc/protocols
COPY	SRCROOT/etc/services	etc/services
COPY	SRCROOT/etc/nteconfig	etc/netconfig

SPECIAL	pwd_mkdb -p -d ./ etc/master.passwd
SPECIAL /bin/rm etc/spwd.db
SPECIAL /bin/rm etc/pwd.db

# copy the MAKEDEV script and make some devices
COPY	SRCROOT/etc/etc.MACHINE/MAKEDEV	dev/MAKEDEV
SPECIAL	cd dev; sh MAKEDEV ramdisk
SPECIAL	/bin/rm dev/MAKEDEV

# we need the boot block in /usr/mdec + the arch specific extras
ifelse(MACHINE,sparc, COPY ${DESTDIR}/usr/mdec/boot usr/mdec/boot)
ifelse(MACHINE,sparc, COPY ${DESTDIR}/usr/mdec/bootxx usr/mdec/bootxx)
ifelse(MACHINE,sparc, COPY ${DESTDIR}/usr/mdec/binstall usr/mdec/binstall)
ifelse(MACHINE,sparc64, COPY ${DESTDIR}/usr/mdec/ofwboot usr/mdec/ofwboot)
ifelse(MACHINE,sparc64, COPY ${DESTDIR}/usr/mdec/ofwboot ofwboot)
ifelse(MACHINE,sparc64, COPY ${DESTDIR}/usr/mdec/bootblk usr/mdec/bootblk)
ifelse(MACHINE,sparc64, COPY ${DESTDIR}/usr/mdec/binstall usr/mdec/binstall)
ifelse(MACHINE,i386,  COPY ${DESTDIR}/usr/mdec/biosboot.sym usr/mdec/biosboot.sym)
ifelse(MACHINE,i386,  COPY ${DESTDIR}/usr/mdec/mbr         usr/mdec/mbr)
ifelse(MACHINE,i386,  COPY ${DESTDIR}/usr/mdec/mbr_bootsel	usr/mdec/mbr_bootsel)

# and the common installation tools
COPY	termcap.mini		usr/share/misc/termcap

# the disktab explanation file
COPY	disktab.preinstall		etc/disktab.preinstall

# zoneinfo files
COPYDIR	${DESTDIR}/usr/share/zoneinfo	usr/share/zoneinfo

#the lists of obsolete files used by sysinst
COPY dist/base_obsolete dist/base_obsolete
COPY dist/comp_obsolete dist/comp_obsolete
COPY dist/games_obsolete dist/games_obsolete
COPY dist/man_obsolete dist/man_obsolete
COPY dist/misc_obsolete dist/misc_obsolete
COPY dist/secr_obsolete dist/secr_obsolete
COPY dist/xbase_obsolete dist/xbase_obsolete
COPY dist/xserver_obsolete dist/xserver_obsolete

# and the installation tools
COPY	${OBJDIR}/dot.profile			.profile

#the lists of obsolete files used by sysinst  
SPECIAL sh ${CURDIR}/../../sets/makeobsolete -b -s ${CURDIR}/../../sets -t ./dist

ifelse(MACHINE,sparc64, SPECIAL gzip -9 < ${SRCDIR}/sys/arch/sparc64/compile/GENERIC/netbsd > netbsd)

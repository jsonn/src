#	$NetBSD: install.md,v 1.1.1.1.4.2 1996/06/28 22:11:20 leo Exp $
#
#
# Copyright (c) 1996 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Jason R. Thorpe.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

#
# machine dependent section of installation/upgrade script.
#

__mount_kernfs() {
	#
	# Mount root rw for convenience of the tester ;-)
	# Force kern_fs to be mounted
	#
	if [ ! -d /kern -o ! -e /kern/msgbuf ]; then
		mount /dev/rd0 / > /dev/null 2>&1
		mkdir /kern > /dev/null 2>&1
		/sbin/mount_kernfs /kern /kern >/dev/null 2>&1
	fi
}

md_set_term() {
	if [ ! -z "$TERM" ]; then
		return
	fi
	echo -n "Specify terminal type [vt220]: "
	getresp "vt220"
	TERM="$resp"
	export TERM
}

md_get_diskdevs() {
    # return available disk devices
    __mount_kernfs
    sed -e '/^sd[0-9] /!d' -e 's/^\(sd[0-9]\) .*/\1/' < /kern/msgbuf | sort -u
}

md_get_cddevs() {
    # return available CDROM devices
    __mount_kernfs
    sed -e '/^cd[0-9] /!d' -e 's/^\(cd[0-9]\) .*/\1/' < /kern/msgbuf | sort -u
}

md_get_ifdevs() {
    # return available network devices
    __mount_kernfs
    sed -e '/^[li]e[0-9] /!d' -e 's/^\([li]e[0-9]\) .*/\1/' < /kern/msgbuf |
		sort -u
}

md_get_partition_range() {
	# return an expression describing the valid partition id's on the Atari
	echo '[a-p]'
}

md_installboot() {
	if [ -x /mnt/usr/mdec/installboot ]; then
		echo "Installing boot block..."
		chroot /mnt /usr/mdec/installboot -v $1
	elif [ "$MODE" = "install" ]; then
		cat << \__md_installboot_1
There is no installboot program found on the installed filesystems. No boot
programs are installed.
__md_installboot_1
	else
		cat << \__md_installboot_2
There is no installboot program found on the upgraded filesystems. No boot
programs are installed.
__md_installboot_2
	fi
}

md_native_fstype() {
	echo "msdos"
}

md_native_fsopts() {
	echo "-G,ro"
}

md_prep_disklabel()
{
	# $1 is the root disk
	# Note that the first part of this function is just a *very* verbose
	# version of md_label_disk().

	cat << \__md_prep_disklabel_1
You now have to prepare your root disk for the installation of NetBSD. This
is further referred to as 'labeling' a disk.

Hit the <return> key when you have read this...
__md_prep_disklabel_1
	getresp ""

	edahdi /dev/r${1}c < /dev/null > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		cat << \__md_prep_disklabel_2
The disk you wish to install on is partitioned with AHDI or an AHDI compatible
program. You have to assign some partitions to NetBSD before NetBSD is able
to use the disk. Change the 'id' of all partitions you want to use for NetBSD
filesystems to 'NBD'. Change the 'id' of the partition you wish to use for swap
to 'NBS' or 'SWP'.

Hit the <return> key when you have read this...
__md_prep_disklabel_2
		getresp ""
 		edahdi /dev/r${1}c
	fi

	# display example
	cat << \__md_prep_disklabel_3
Here is an example of what the partition information will look like once
you have entered the disklabel editor. Disk partition sizes and offsets
are in sector (most likely 512 bytes) units.

[Example]
partition      start         (c/t/s)      nblks         (c/t/s)  type

 a (root)          0       (0/00/00)      31392     (109/00/00)  4.2BSD
 b (swap)      31392     (109/00/00)      73440     (255/00/00)  swap
 c (disk)          0       (0/00/00)    1070496    (3717/00/00)  unused
 d (user)     104832     (364/00/00)      30528     (106/00/00)  4.2BSD
 e (user)     135360     (470/00/00)      40896     (142/00/00)  4.2BSD
 f (user)     176256     (612/00/00)      92160     (320/00/00)  4.2BSD
 g (user)     268416     (932/00/00)     802080    (2785/00/00)  4.2BSD

[End of example]

Hit the <return> key when you have read this...

__md_prep_disklabel_3
	getresp ""
	edlabel /dev/r${1}c

	cat << \__md_prep_disklabel_4

You will now be given the opportunity to place disklabels on any additional
disks on your system.
__md_prep_disklabel_4

	_DKDEVS=`rmel ${1} ${_DKDEVS}`
	resp="X"	# force at least one iteration
	while [ "X$resp" != X"done" ]; do
		labelmoredisks
	done
}

md_labeldisk() {
	edahdi /dev/r${1}c < /dev/null > /dev/null 2>&1
	[ $? -eq 0 ] && edahdi /dev/r${1}c
	edlabel /dev/r${1}c
}

md_copy_kernel() {
	if [ ! -e /netbsd ]; then
cat << \__md_copy_kernel_1
Your installation set did not include a netbsd kernel on the installation
filesystem. You have to install this yourself after you have booted your
newly installed filesystems. See the INSTALL document for further information.

Hit the <return> key when you have read this...
__md_copy_kernel_1
		getresp ""
	else
		echo -n "Copying kernel..."
		cp -p /netbsd /mnt/netbsd
		echo "done."
	fi
}

md_welcome_banner() {
	if [ "$MODE" = "install" ]; then
		echo ""
		echo "Welcome to the NetBSD/atari ${VERSION} installation program."
		cat << \__welcome_banner_1

This program is designed to help you put NetBSD on your disk,
in a simple and rational way.  You'll be asked several questions,
and it would probably be useful to have your disk's hardware
manual, the installation notes, and a calculator handy.
__welcome_banner_1

	else
		echo ""
		echo "Welcome to the NetBSD/atari ${VERSION} upgrade program."
		cat << \__welcome_banner_2

This program is designed to help you upgrade your NetBSD system in a
simple and rational way.

As a reminder, installing the `etc' binary set is NOT recommended.
Once the rest of your system has been upgraded, you should manually
merge any changes to files in the `etc' set into those files which
already exist on your system.
__welcome_banner_2
	fi

cat << \__welcome_banner_3

As with anything which modifies your disk's contents, this
program can cause SIGNIFICANT data loss, and you are advised
to make sure your data is backed up before beginning the
installation process.

Default answers are displayed in brackets after the questions.
You can hit Control-C at any time to quit, but if you do so at a
prompt, you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__welcome_banner_3
}

md_not_going_to_install() {
	cat << \__not_going_to_install_1

OK, then.  Enter `halt' at the prompt to halt the machine.  Once the
machine has halted, power-cycle the system to load new boot code.

Note: If you wish to have another try. Just type '^D' at the prompt. After
      a moment, the installer will restart itself.

__not_going_to_install_1
}

md_congrats() {
	local what;
	if [ "$MODE" = "install" ]; then
		what="installed";
	else
		what="upgraded";
	fi
	cat << __congratulations_1

CONGRATULATIONS!  You have successfully $what NetBSD!
To boot the installed system, enter halt at the command prompt. Once the
system has halted, reset the machine and boot from the disk.

Note: If you wish to have another try. Just type '^D' at the prompt. After
      a moment, the installer will restart itself.

__congratulations_1
}

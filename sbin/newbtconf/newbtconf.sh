#!/bin/sh
#
# Setup a new config directory
#
if [ $# -lt 1 ] ; then
	echo "Usage: $0 <newconfig> [<baseconfig>]"
	echo "Usage: $0 init"
	exit 1;
fi
dir=$1

FILES="defaultdomain fstab ifconfig.* inetd.conf mrouted.conf \
	mygate myname netstart nsswitch.conf ntp.conf rbootd.conf \
	rc.conf rc.conf.d resolv.conf"

if [ $dir = init ] ; then
	if [ -d /etc/etc.network -o -e /etc/etc/current ] ; then
		echo "Error: multi-configuration already initialized"
		exit 1
	fi
	dir=etc.network
	cd /etc
	mkdir -m 755 $dir
	ln -s $dir etc.current
	ln -s $dir etc.default
	for i in ${FILES}; do
		if [ -f $i ] ; then
			mv $i $dir
			ln -s etc.current/$i .
		fi
	done
	echo "/etc/$dir has now been created and populated."
	exit 0
fi

if [ "`expr $dir : 'etc\.\(.*\)'`" != $dir ] ; then
	dir=etc.$dir
fi
if [ -e /etc/$dir ] ; then
	echo "Error: $dir already exists"
	exit 1;
fi
newname=`expr $dir : 'etc.\(.*\)'`
if [ $# -lt 2 ] ; then
	orig=etc.current
	echo "Using current config as base for $newname"
else
	orig=$2
fi

if [ -z "`expr $orig : 'etc.\(.*\)'`" ] ; then
	orig=etc.$orig
fi

if [ ! -d /etc/$orig ] ; then
	echo "Original directory /etc/$orig does not exist."
	exit 1;
fi
mkdir -m 755 /etc/$dir
cp -p /etc/$orig/* /etc/$dir
echo "/etc/$dir has now been created and populated."
exit 0

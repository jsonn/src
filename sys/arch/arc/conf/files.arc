#	$NetBSD: files.arc,v 1.19.2.1 2000/06/22 16:59:10 minoura Exp $
#	$OpenBSD: files.arc,v 1.21 1999/09/11 10:20:20 niklas Exp $
#
# maxpartitions must be first item in files.${ARCH}
#
maxpartitions 16

maxusers 2 8 64

#	Required files

file	arch/arc/arc/autoconf.c
file	arch/arc/arc/conf.c
file	arch/arc/arc/disksubr.c
file	arch/arc/dev/dma.c
file	arch/arc/arc/machdep.c
#file	arch/arc/arc/minidebug.c
file	arch/arc/arc/arc_trap.c
file	arch/arc/arc/bus_space.c
file	arch/arc/arc/bus_space_sparse.c
file	arch/arc/arc/bus_space_large.c
file	arch/arc/arc/bus_dma.c
file	arch/arc/arc/wired_map.c

file	arch/arc/arc/arcbios.c

##
##	Machine-independent ATAPI drivers 
##
include "dev/ata/files.ata"
major	{ wd = 4 }

# Raster operations
include "dev/rasops/files.rasops"
include "dev/wsfont/files.wsfont"

#
# "Workstation Console" glue.
#
include "dev/wscons/files.wscons"

include "dev/pckbc/files.pckbc"

#
#	System BUS types
#
device mainbus { }			# no locators
attach mainbus at root
file	arch/arc/arc/mainbus.c	mainbus

#	Our CPU configurator
device cpu				# not optional
attach cpu at mainbus
file arch/arc/arc/cpu.c			cpu

#
#	Magnum and PICA bus autoconfiguration devices
#
device	pica {}
attach	pica at mainbus			# optional
file	arch/arc/pica/picabus.c		pica
file	arch/arc/jazz/jazzdmatlb.c	# XXX pica
file	arch/arc/jazz/bus_dma_jazz.c	# XXX pica

#
#	ALGOR bus autoconfiguration devices
#
device	algor {}
attach	algor at mainbus		# optional
file	arch/arc/algor/algorbus.c	algor

#
#	ISA Bus bridge
#
device	isabr {} : isabus
attach	isabr at mainbus		# optional
file	arch/arc/isa/isabus.c		isabr
file	arch/arc/isa/isadma_bounce.c	# XXX DESKSTATION_RPC44

#
#	PCI Bus bridge
#
device	pbcpcibr {} : pcibus
attach	pbcpcibr at mainbus		# optional
file	arch/arc/pci/pbcpcibus.c	pbcpcibr

#
#	NEC RISCstation PCI host bridge
#
device	necpb: pcibus
attach	necpb at mainbus		# optional
file	arch/arc/pci/necpb.c		necpb

#	Ethernet chip on PICA bus
device	sn: ifnet, ether, arp
attach	sn at pica
file	arch/arc/dev/if_sn.c		sn

#
# Machine-independent MII/PHY drivers.
#
include "dev/mii/files.mii"

#	Use machine independent SCSI driver routines
include	"dev/scsipi/files.scsipi"
major	{sd = 0}
major	{cd = 3}

#	Symbios 53C94 SCSI interface driver on PICA bus
device	asc: scsi
attach	asc at pica
file	arch/arc/dev/asc.c		asc

#	Floppy disk controller on PICA bus
device	fdc {drive = -1}
attach	fdc at pica
device	fd: disk
attach	fd at fdc
file	arch/arc/dev/fd.c		fdc	needs-flag
major	{fd = 7}

#	bus independent raster console glue
device	rasdisplay: wsemuldisplaydev, pcdisplayops
file	arch/arc/dev/rasdisplay.c	rasdisplay

#	raster console glue on PICA bus
attach	rasdisplay at pica with rasdisplay_jazzio
file	arch/arc/jazz/rasdisplay_jazzio.c rasdisplay_jazzio needs-flag

#	VGA display driver on PICA bus
attach	vga at pica with vga_jazzio
file	arch/arc/jazz/vga_jazzio.c	vga_jazzio needs-flag

#	PC keyboard controller on PICA bus
attach  pckbc at pica with pckbc_jazzio
file    arch/arc/jazz/pckbc_jazzio.c	pckbc_jazzio needs-flag

#
#	Stock ISA bus support
#
define	pcmcia {}			# XXX dummy decl...

include	"dev/pci/files.pci"
include	"dev/isa/files.isa"

file arch/arc/pci/pciide_machdep.c		pciide

#	Real time clock, must have one..
device	aclock
attach	aclock at pica with aclock_pica
attach	aclock at isa with aclock_isa
attach	aclock at algor with aclock_algor
file	arch/arc/arc/clock.c	aclock & (aclock_isa | aclock_pica | aclock_algor) needs-flag
file	arch/arc/arc/clock_mc.c	aclock & (aclock_isa | aclock_pica | aclock_algor) needs-flag

#	Console driver on PC-style graphics
device	pc: tty
attach	pc at pica with pc_pica
attach	pc at isa with pc_isa
device	opms: tty
attach	opms at pica
file	arch/arc/dev/pccons.c	pc & (pc_pica | pc_isa | opms) needs-flag

#	BusLogic BT-445C VLB SCSI Controller. Special on TYNE local bus.
device	btl: scsi
attach	btl at isa
file	arch/arc/dti/btl.c		btl

#	NS16450/16550 Serial line driver
attach	com at pica with com_pica
attach	com at algor with com_algor
file	arch/arc/dev/com_lbus.c		com & (com_pica | com_algor)

# Game adapter (joystick)
device	joy
attach	joy at isa
file	arch/arc/isa/joy.c		joy needs-flag

# National Semiconductor DS8390/WD83C690-based boards
# (WD/SMC 80x3 family, SMC Ultra [8216], 3Com 3C503, NE[12]000, and clones)
# XXX conflicts with other ports; can't be in files.isa
## XXX: should fix conflict with files.isa
#device	ed: ether, ifnet
#attach	ed at isa with ed_isa
#attach	ed at pcmcia with ed_pcmcia
#file	dev/isa/if_ed.c			ed & (ed_isa | ed_pcmcia) needs-flag

#	Parallel printer port driver
attach	lpt at pica with lpt_pica
attach	lpt at algor with lpt_algor
file	arch/arc/dev/lpt_lbus.c		lpt & (lpt_pica | lpt_algor)


#
#	PCI Bus support
#

# PCI VGA display driver
device	pcivga: tty
attach	pcivga at pci
file	arch/arc/pci/pci_vga.c		pcivga

#
# Specials.
#
# memory disk for installation
file arch/arc/dev/md_root.c		memory_disk_hooks
major {md = 8}

# RAIDframe
major {raid = 9}

#
#	Common files
#

file	dev/cons.c
#file	dev/cninit.c
#file	netinet/in_cksum.c
#file	netns/ns_cksum.c			ns

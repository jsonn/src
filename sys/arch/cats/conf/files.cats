#	$NetBSD: files.cats,v 1.10.6.7 2002/11/11 21:57:34 nathanw Exp $
#
# CATS-specific configuration info
#

maxpartitions	8
maxusers 2 8 64

# Maintain Interrupt statistics
defflag	IRQSTATS

# X server support in console drivers
defflag	XSERVER

define todservice {}

#
# ISA and mixed ISA+EISA or ISA+PCI drivers
#
include "dev/isa/files.isa"

# Include arm32 footbridge
include "arch/arm/conf/files.footbridge"

#
# Machine-independent ATA drivers
#
include "dev/ata/files.ata"

#
# time of day clock
#
device	todclock
attach	todclock at todservice
file	arch/arm/footbridge/todclock.c			todclock	needs-count

# ISA DMA glue
file	arch/arm/footbridge/isa/isadma_machdep.c	isadma

# Game adapter (joystick)
file	arch/arm/footbridge/isa/joy_timer.c		joy

# Memory disk driver
file	dev/md_root.c				md & memory_disk_hooks

#
# Machine-independent SCSI/ATAPI drivers
#

include "dev/scsipi/files.scsipi"

# Generic MD files
file	arch/cats/cats/autoconf.c
file	arch/cats/cats/cats_machdep.c

# library functions

file	arch/arm/arm/disksubr.c			disk
file	arch/arm/arm/disksubr_acorn.c		disk
file	arch/arm/arm/disksubr_mbr.c		disk

# ISA Plug 'n Play autoconfiguration glue.
file	arch/arm/footbridge/isa/isapnp_machdep.c	isapnp

# ISA support.
file	arch/arm/footbridge/isa/isa_io.c		isa
file	arch/arm/footbridge/isa/isa_io_asm.S		isa

# CATS boards have an EBSA285 based core with an ISA bus
file	arch/arm/footbridge/isa/isa_machdep.c		isa

device	sysbeep
attach	sysbeep at pcppi with sysbeep_isa
file	arch/arm/footbridge/isa/sysbeep_isa.c		sysbeep_isa

device dsrtc: todservice
attach dsrtc at isa
file	arch/arm/footbridge/isa/dsrtc.c			dsrtc
# Machine-independent I2O drivers.
include "dev/i2o/files.i2o"

# PCI devices

#
# Include PCI config
#
include "dev/pci/files.pci"

device	pcib: isabus
attach	pcib at pci
file	arch/cats/pci/pcib.c			pcib

# XXX THE FOLLOWING BLOCK SHOULD GO INTO dev/pci/files.pci, BUT CANNOT
# XXX BECAUSE NOT 'lpt' IS DEFINED IN files.isa, RATHER THAN files.
# XXX (when the conf/files and files.isa bogons are fixed, this can
# XXX be fixed as well.)

attach	lpt at puc with lpt_puc
file	dev/pci/lpt_puc.c	lpt_puc

file	arch/cats/pci/pciide_machdep.c	pciide

# Include USB stuff
include "dev/usb/files.usb"

# Include WSCONS stuff
include "dev/wscons/files.wscons"
include "dev/rasops/files.rasops"
include "dev/wsfont/files.wsfont"
include "dev/pckbc/files.pckbc"

include "arch/arm/conf/majors.arm32"

#	$NetBSD: std.arc,v 1.15.2.2 2004/09/18 14:32:08 skrll Exp $
# standard arc info

machine arc mips
makeoptions	MACHINE_ARCH="mipsel"

mainbus0 at root
cpu* at mainbus0

# set CPU architecture level for kernel target
#options 	MIPS1			# R2000/R3000 support
options 	MIPS3			# R4000/R4400 support

# Standard (non-optional) system "options"

# Standard exec-package options
options 	EXEC_ELF32		# native exec format
options 	EXEC_SCRIPT		# may be unsafe

makeoptions	DEFTEXTADDR="0x80200000"

#	$NetBSD: files.viper,v 1.1.8.2 2005/11/10 13:55:51 skrll Exp $
#
# Arcom Viper
#

# homegrown stuff since initarm_common seems dead
file	arch/evbarm/viper/viper_machdep.c

# CPU and I/O
include	"arch/arm/xscale/files.pxa2x0"

# SMSC LAN91C111
attach	sm at pxaip with sm_pxaip
file	arch/evbarm/viper/if_sm_pxaip.c		sm_pxaip

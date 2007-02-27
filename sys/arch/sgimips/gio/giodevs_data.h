/*	$NetBSD: giodevs_data.h,v 1.5.10.1 2007/02/27 16:52:56 yamt Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 */


struct gio_knowndev {
	int productid;
	const char *product;
};

struct gio_knowndev gio_knowndevs[] = {
	{ 0x01, "XPI low cost FDDI" },
	{ 0x02, "GTR TokenRing" },
	{ 0x04, "Synchronous ISDN" },
	{ 0x06, "Canon Interface" },
	{ 0x08, "JPEG (Double Wide)" },
	{ 0x09, "JPEG (Single Wide)" },
	{ 0x0a, "XPI mez. FDDI device 0" },
	{ 0x0b, "XPI mez. FDDI device 1" },
	{ 0x0e, "E-Plex 8-port Ethernet" },
	{ 0x30, "Lyon Lamb IVAS" },
	{ 0x35, "Phobos G160 10/100 Ethernet" },
	{ 0x36, "Phobos G130 10/100 Ethernet" },
	{ 0x37, "Phobos G100 100baseTX Fast Ethernet" },
	{ 0x38, "Set Engineering GFE 10/100 Ethernet" },
	{ 0x85, "ATM board" },
	{ 0x87, "16 bit SCSI Card" },
	{ 0x8c, "SMPTE 259M Video" },
	{ 0x8d, "Babblefish Compression" },
	{ 0, NULL }
};

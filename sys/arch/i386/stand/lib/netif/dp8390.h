/*	$NetBSD: dp8390.h,v 1.4.2.2 2001/11/01 09:37:18 yamt Exp $	*/

extern int dp8390_config __P((void));
extern void dp8390_stop __P((void));

extern int dp8390_iobase;
extern int dp8390_membase;
extern int dp8390_memsize;
#ifdef SUPPORT_WD80X3
#ifdef SUPPORT_SMC_ULTRA
extern int dp8390_is790;
#else
#define dp8390_is790 0
#endif
#else
#ifdef SUPPORT_SMC_ULTRA
#define dp8390_is790 1
#endif
#endif

#ifdef SUPPORT_NE2000
#define dp8390_is790 0
#define IFNAME "ne"
#define RX_BUFBASE 0
#define TX_PAGE_START (dp8390_membase >> ED_PAGE_SHIFT)
#else
#define IFNAME "we"
#define RX_BUFBASE dp8390_membase
#define TX_PAGE_START 0
#endif

extern u_int8_t dp8390_cr_proto; /* values always set in CR */
extern u_int8_t dp8390_dcr_reg; /* override DCR if LS is set */

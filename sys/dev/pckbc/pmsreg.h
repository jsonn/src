/* $NetBSD: pmsreg.h,v 1.1.8.2 2002/06/23 17:48:10 jdolecek Exp $ */

/* mouse commands */
#define	PMS_SET_SCALE11	0xe6	/* set scaling 1:1 */
#define	PMS_SET_SCALE21 0xe7	/* set scaling 2:1 */
#define	PMS_SET_RES	0xe8	/* set resolution (0..3) */
#define	PMS_GET_SCALE	0xe9	/* get scaling factor */
#define PMS_SEND_DEV_STATUS	0xe9
#define	PMS_SET_STREAM	0xea	/* set streaming mode */
#define PMS_SEND_DEV_DATA	0xeb
#define PMS_SET_REMOTE_MODE	0xf0
#define PMS_SEND_DEV_ID	0xf2
#define	PMS_SET_SAMPLE	0xf3	/* set sampling rate */
#define	PMS_DEV_ENABLE	0xf4	/* mouse on */
#define	PMS_DEV_DISABLE	0xf5	/* mouse off */
#define PMS_SET_DEFAULTS	0xf6
#define	PMS_RESET	0xff	/* reset */

#define	PMS_RSTDONE	0xaa

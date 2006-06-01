/*	$NetBSD: scsi_low.c,v 1.5.6.1 2006/06/01 22:35:09 kardel Exp $	*/

/****************************************************************************
 * NS32K Monitor SCSI low-level driver
 * Bruce Culbertson
 * 8 March 1990
 * (This source is public domain source.)
 *
 * Originally written by Bruce Culbertson for a ns32016 port of Minix.
 * Adapted from that for the pc532 (ns32632) monitor.
 * Adapted from that for NetBSD/pc532 by Philip L. Bunde.
 *
 * Do not use DMA -- makes 32016 and pc532 versions compatible.
 * Do not use interrupts -- makes it harder for the user code to bomb
 * this code.
 ****************************************************************************/

#include <lib/libsa/stand.h>

#include <pc532/stand/common/so.h>

#define	OK		0
#define	NOT_OK		OK+1
#define	PRIVATE
#define	PUBLIC
#define	WR_ADR(adr,val)	(*((volatile unsigned char *)(adr))=(val))
#define	RD_ADR(adr)	(*((volatile unsigned char *)(adr)))
#define	AIC6250		0
#define	DP8490		1
#define	MAX_CACHE	0x10000

/* SCSI bus phases
 */
#define	PH_ODATA	0
#define	PH_IDATA	1
#define	PH_CMD		2
#define	PH_STAT		3
#define	PH_IMSG		7
#define	PH_NONE		8
#define	PH_IN(phase)	((phase) & 1)

/* NCR5380 SCSI controller registers
 */
#define	SC_CTL		0x30000000	/* base for control registers */
#define	SC_DMA		0x38000000	/* base for data registers */
#define	SC_CURDATA	SC_CTL+0
#define	SC_OUTDATA	SC_CTL+0
#define	SC_ICMD		SC_CTL+1
#define	SC_MODE		SC_CTL+2
#define	SC_TCMD		SC_CTL+3
#define	SC_STAT1	SC_CTL+4
#define	SC_STAT2	SC_CTL+5
#define	SC_START_SEND	SC_CTL+5
#define	SC_INDATA	SC_CTL+6
#define	SC_RESETIP	SC_CTL+7
#define	SC_START_RCV	SC_CTL+7

/* Bits in NCR5380 registers
 */
#define	SC_A_RST	0x80
#define	SC_A_SEL	0x04
#define	SC_S_SEL	0x02
#define	SC_S_REQ	0x20
#define	SC_S_BSY	0x40
#define	SC_S_BSYERR	0x04
#define	SC_S_PHASE	0x08
#define	SC_S_IRQ	0x10
#define	SC_S_DRQ	0x40
#define	SC_M_DMA	0x02
#define	SC_M_BSY	0x04
#define	SC_ENABLE_DB	0x01

/* Status of interrupt routine, returned in m1_i1 field of message.
 */
#define	ISR_NOTDONE	0
#define	ISR_OK		1
#define	ISR_BSYERR	2
#define	ISR_RSTERR	3
#define	ISR_BADPHASE	4
#define	ISR_TIMEOUT	5

#define	ICU_ADR		0xfffffe00
#define	ICU_IO		(ICU_ADR+20)
#define	ICU_DIR		(ICU_ADR+21)
#define	ICU_DATA	(ICU_ADR+19)
#define	ICU_SCSI_BIT	0x80

/* Miscellaneous
 */
#define	MAX_WAIT	2000000
#define	SC_LOG_LEN	32

PRIVATE struct scsi_args	*sc_ptrs;
PRIVATE unsigned char		sc_cur_phase,
				sc_reset_done = 1,
				sc_have_msg,
				sc_accept_int,
				sc_dma_dir;

long	sc_dma_port = SC_DMA,
	sc_dma_adr;

#ifdef DEBUG
struct sc_log {
	unsigned char stat1, stat2;
}				sc_log [SC_LOG_LEN],
				*sc_log_head = sc_log;
int				sc_spurious_int;
#endif
unsigned char sc_watchdog_error;		/* watch dog error */

/* error messages */
char *scsi_errors[] = {
	0,					/* ISR_NOTDONE */
	0,					/* ISR_OK */
	"busy error",				/* ISR_BSYERR */
	"reset error",				/* ISR_RSTERR */
	"NULL pointer for current phase",	/* ISR_BADPHASE */
	"timeout",				/* ISR_TIMEOUT */
};

PRIVATE void sc_reset(void);
void scCtlrSelect(int);
PRIVATE int sc_wait_bus_free(void);
void sc_dma_setup(int, long);
int sc_receive(void);
PRIVATE int sc_select(long);
PUBLIC int scsi_interrupt(void);

/*===========================================================================*
 *				exec_scsi_low				     *
 *===========================================================================*/
/* Execute a generic SCSI command.  Passed pointers to eight buffers:
 * data-out, data-in, command, status, dummy, dummy, message-out, message-in.
 */
PUBLIC int
exec_scsi_low(struct scsi_args *args, long scsi_adr)
{
	int ret;

	sc_ptrs = args;			/* make pointers globally accessible */
	scCtlrSelect(DP8490);
	if (!sc_reset_done)
		sc_reset();
	/*
	 * TCMD has some undocumented behavior in initiator mode.  I think the
	 * data bus cannot be enabled if i/o is asserted.
	 */
	WR_ADR(SC_TCMD, 0);
	if (OK != sc_wait_bus_free()) {	/* bus-free phase */
		printf("SCSI: bus not free\n");
		return NOT_OK;
	}
	sc_cur_phase = PH_NONE;
	sc_have_msg = 0;
	if (OK != sc_select(scsi_adr))	/* select phase */
		return NOT_OK;
	sc_watchdog_error = 0;
	ret = sc_receive();		/* isr does the rest */
	if (ret == ISR_OK)
		return OK;
	else {
		sc_reset();
		printf("SCSI: %s\n", scsi_errors[ret]);
		return NOT_OK;
	}
}

/*===========================================================================*
 *				sc_reset				     *
 *===========================================================================*/
/*
 * Reset SCSI bus.
 */
PRIVATE
void sc_reset(void)
{
	volatile int i;

	WR_ADR(SC_MODE, 0);			/* get into harmless state */
	WR_ADR(SC_OUTDATA, 0);
	WR_ADR(SC_ICMD, SC_A_RST);		/* assert RST on SCSI bus */
	i = 200;				/* wait 25 usec */
	while (i--)
		/* nothing */;
	WR_ADR(SC_ICMD, 0);			/* deassert RST, get off bus */
	sc_reset_done = 1;
}

/*===========================================================================*
 *				sc_wait_bus_free			     *
 *===========================================================================*/
PRIVATE int
sc_wait_bus_free(void)
{
	int i = MAX_WAIT;
	volatile int j;

	while (i--) {
		/* Must be clear for 2 usec, so read twice */
		if (RD_ADR(SC_STAT1) & (SC_S_BSY | SC_S_SEL))
			continue;
		for (j = 0; j < 25; ++j)
			/* nothing */;
		if (RD_ADR(SC_STAT1) & (SC_S_BSY | SC_S_SEL))
			continue;
		return OK;
	}
	sc_reset_done = 0;
	return NOT_OK;
}

/*===========================================================================*
 *				sc_select				     *
 *===========================================================================*/
/* This duplicates much of the work that the interrupt routine would do on a
 * phase mismatch and, in fact, the original plan was to just do the select,
 * let a phase mismatch occur, and let the interrupt routine do the rest.
 * That didn't work because the 5380 did not reliably generate the phase
 * mismatch interrupt after selection.
 */
PRIVATE int
sc_select(long adr)
{
	int i, stat1;
	long new_ptr;

	WR_ADR(SC_OUTDATA, adr);	/* SCSI bus address */
	WR_ADR(SC_ICMD, SC_A_SEL | SC_ENABLE_DB);
	for (i = 0;; ++i) {		/* wait for target to assert SEL */
		stat1 = RD_ADR(SC_STAT1);
		if (stat1 & SC_S_BSY)
			break;		/* select successful */
		if (i > MAX_WAIT) {	/* timeout */
			printf("SCSI: SELECT timeout\n");
			sc_reset();
			return NOT_OK;
		}
	}
	WR_ADR(SC_ICMD, 0);		/* clear SEL, disable data out */
	WR_ADR(SC_OUTDATA, 0);
	for (i = 0;; ++i) {		/* wait for target to assert REQ */
		if (stat1 & SC_S_REQ)
			break;		/* target requesting transfer */
		if (i > MAX_WAIT) {	/* timeout */
			printf("SCSI: REQ timeout\n");
			sc_reset();
			return NOT_OK;
		}
		stat1 = RD_ADR(SC_STAT1);
	}
	sc_cur_phase = (stat1 >> 2) & 7; /* get new phase from controller */
	if (sc_cur_phase != PH_CMD) {
		printf("SCSI: bad phase = %d\n", sc_cur_phase);
		sc_reset();
		return NOT_OK;
	}
	new_ptr = sc_ptrs->ptr[PH_CMD];
	if (new_ptr == 0) {
		printf("SCSI: NULL command pointer\n");
		sc_reset();
		return NOT_OK;
	}
	sc_accept_int = 1;
	sc_dma_setup(DISK_WRITE, new_ptr);
	WR_ADR(SC_TCMD, PH_CMD);
	WR_ADR(SC_ICMD, SC_ENABLE_DB);
	WR_ADR(SC_MODE, SC_M_BSY | SC_M_DMA);
	WR_ADR(SC_START_SEND, 0);
	return OK;
}

/*===========================================================================*
 *				scsi_interrupt				     *
 *===========================================================================*/
/* SCSI interrupt handler.
 */
PUBLIC int
scsi_interrupt(void)
{
	unsigned char stat2, dummy;
	long new_ptr;
	int ret = ISR_NOTDONE;

	stat2 = RD_ADR(SC_STAT2);	/* get status before clearing request */

#ifdef DEBUG				/* debugging log of interrupts */
	sc_log_head->stat1 = RD_ADR(SC_STAT1);
	sc_log_head->stat2 = stat2;
	if (++sc_log_head >= sc_log + SC_LOG_LEN)
		sc_log_head = sc_log;
	sc_log_head->stat1 = sc_log_head->stat2 = 0xff;
#endif

	for (;;) {
		dummy = RD_ADR(SC_RESETIP);	/* clear interrupt request */
		if (!sc_accept_int ||	/* return if spurious interrupt */
		    (!sc_watchdog_error && (stat2 & SC_S_BSYERR) == 0 &&
		    (stat2 & SC_S_PHASE) == SC_S_PHASE)) {
#ifdef DEBUG
			++sc_spurious_int;
#endif
			return ret;
		}
		RD_ADR(SC_MODE) &= ~SC_M_DMA;	/* clear DMA mode */
		WR_ADR(SC_ICMD, 0);		/* disable data bus */
		if (sc_cur_phase != PH_NONE) {	/* if did DMA, save the new
								pointer */
			/* fetch new pointer from DMA cntlr */
			new_ptr = sc_dma_adr;
			if (sc_cur_phase == PH_IMSG &&
			    new_ptr != sc_ptrs->ptr[PH_IMSG]) /* have message? */
				sc_have_msg = 1;
			sc_ptrs->ptr[sc_cur_phase] = new_ptr; /* save pointer */
		}
		if (sc_watchdog_error)
			ret = ISR_TIMEOUT;
		else if (stat2 & SC_S_BSYERR) {	/* target deasserted BSY? */
			if (sc_have_msg)
				ret = ISR_OK;
			else
				ret = ISR_BSYERR;
		} else if (!(stat2 & SC_S_PHASE)) {	/* if phase mismatch,
							setup new phase */
			/* get new phase from controller */
			sc_cur_phase = (RD_ADR(SC_STAT1) >> 2) & 7;
			new_ptr = sc_ptrs->ptr[sc_cur_phase];
			if (new_ptr == 0)
				ret = ISR_BADPHASE;
			else {
				/* write new phase into TCMD */
				WR_ADR(SC_TCMD, sc_cur_phase);
				if (PH_IN(sc_cur_phase)) {	/* set DMA
								controller */
					sc_dma_setup(DISK_READ, new_ptr);
					RD_ADR(SC_MODE) |= SC_M_DMA;

					/* tell SCSI to start DMA */
					WR_ADR(SC_START_RCV, 0);
				} else {
					sc_dma_setup(DISK_WRITE, new_ptr);
					RD_ADR(SC_MODE) |= SC_M_DMA;
					WR_ADR(SC_ICMD, SC_ENABLE_DB);
					WR_ADR(SC_START_SEND, 0);
				}
			}
		} else
			ret = ISR_RSTERR;
		if (ret != ISR_NOTDONE) { /* if done, send message to task */
			sc_watchdog_error = 0;
			sc_accept_int = 0;
			WR_ADR(SC_MODE, 0);	/* clear monbsy, DMA */
			break;			/* reti re-enables ints */
		}
		if (0 == ((stat2 = RD_ADR(SC_STAT2)) & SC_S_IRQ)) /* check for
							another interrupt */
			break;
	}
	return ret;
}

/*===========================================================================*
 *				sc_dma_setup				     *
 *===========================================================================*/
/* Fake DMA setup.  Just store pointers and direction in global variables.
 *
 * The pseudo-DMA is subtler than it looks because of the cache.
 *
 * 1)	When accessing I/O devices through a cache, some mechanism is
 *	necessary to ensure you access the device rather than the cache.
 *	On the 32532, the IODEC signal is supposed to be asserted for I/O
 *	addresses to accomplish this.  However, a bug makes this much
 *	slower than necessary and severely hurts pseudo-DMA performance.
 *	Hence, IODEC is not asserted for the SCSI DMA port.
 *
 * 2)	Because of (1), we must devise our own method of forcing the
 *	SCSI DMA port to be read.  0x8000000 addresses have been decoded
 *	to all access this port.  By always using new addresses to access
 *	the DMA port (wrapping only after reading MAX_CACHE bytes), we
 *	force cache misses and, hence, device reads.  Since the cache
 *	is write-through, we do not need to worry about writes.
 *
 * 3)	It is possible to miss the last few bytes of a transfer if
 *	bus transfer size is not considered.  The loop in sc_receive()
 *	transfers data until the interrupt signal is asserted.  If
 *	bytes are transferred, the attempt to move the first byte of a
 *	double word causes the whole word to be read into the cache.
 *	Then the byte is transferred.  If reading the double word
 *	completed the SCSI transfer, then the loop exits since
 *	interrupt is asserted.  However, the last few bytes have only
 *	been moved into the cache -- they have not been moved to the
 *	DMA destination.
 *
 * 4)	It is also possible to miss the first few bytes of a transfer.
 *	If the address used to access pseudo-DMA port is not double word
 *	aligned, the whole double word is read into the cache, and then
 *	data is moved from the middle of the word (i.e. something other
 *	than the first bytes read from the SCSI controller) by the
 *	pseudo-DMA loop in sc_receive().
 */
void
sc_dma_setup(int dir, long adr)
{

	if (sc_dma_port > SC_DMA + MAX_CACHE)
		sc_dma_port = SC_DMA;
	sc_dma_dir = dir;
	sc_dma_adr = adr;
}

/*===========================================================================*
 *				sc_receive				     *
 *===========================================================================*/
/* Replacement for Minix receive(), which waits for a message.  This code
 * spins, waiting for data to transfer or interrupt requests to handle.
 * See sc_dma_setup for details.
 */
int
sc_receive(void)
{
	int stat2, isr_ret;

	for (;;) {
		stat2 = RD_ADR(SC_STAT2);
		if (stat2 & SC_S_IRQ) {
			if (ISR_NOTDONE != (isr_ret = scsi_interrupt()))
				break;
		} else if (stat2 & SC_S_DRQ) {	/* test really not necessary on
						   pc532 */
			if (sc_dma_dir == DISK_READ)
				*((long *)sc_dma_adr)++ =
				    *((volatile long *)sc_dma_port)++;
			else
				*((volatile long *)sc_dma_port)++ =
				    *((long *)sc_dma_adr)++;
		}
	}
	return isr_ret;
}

/*===========================================================================*
 *				scCtlrSelect
 *===========================================================================*/
/* Select a SCSI device.
 */
void
scCtlrSelect(int ctlr)
{

	RD_ADR(ICU_IO) &= ~ICU_SCSI_BIT;	/* i/o, not port */
	RD_ADR(ICU_DIR) &= ~ICU_SCSI_BIT;	/* output */
	if (ctlr == DP8490)
		RD_ADR(ICU_DATA) &= ~ICU_SCSI_BIT; /* select = 0 for 8490 */
	else
		RD_ADR(ICU_DATA) |= ICU_SCSI_BIT; /* select = 1 for AIC6250 */
}

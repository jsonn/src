#define PCMCIACISDEBUG

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciachip.h>
#include <dev/pcmcia/pcmciavar.h>

#ifdef PCMCIACISDEBUG
int	pcmciacis_debug = 0;
#define DPRINTF(arg) if (pcmciacis_debug) printf arg
#else
#define DPRINTF(arg)
#endif

#define PCMCIA_CIS_SIZE		1024

struct cis_state {
    int count;
    int gotmfc;
    struct pcmcia_config_entry temp_cfe;
    struct pcmcia_config_entry *default_cfe;
    struct pcmcia_card *card;
    struct pcmcia_function *pf;
};

int pcmcia_parse_cis_tuple __P((struct pcmcia_tuple *, void *));

void
pcmcia_read_cis(sc)
     struct pcmcia_softc *sc;
{
    struct cis_state state;

    state.count = 0;
    state.gotmfc = 0;

    state.card = &sc->card;

    state.card->error = 0;
    state.card->cis1_major = -1;
    state.card->cis1_minor = -1;
    state.card->cis1_info[0] = NULL;
    state.card->cis1_info[1] = NULL;
    state.card->cis1_info[2] = NULL;
    state.card->cis1_info[3] = NULL;
    state.card->manufacturer = -1;
    state.card->product = -1;
    SIMPLEQ_INIT(&state.card->pf_head);

    state.pf = NULL;

    if (pcmcia_scan_cis((struct device *) sc, pcmcia_parse_cis_tuple, &state))
	state.card->error++;
}

int
pcmcia_scan_cis(dev, fct, arg)
     struct device *dev;
     int (*fct) __P((struct pcmcia_tuple *, void *));
     void *arg;
{
    struct pcmcia_softc *sc = (struct pcmcia_softc *) dev;
    pcmcia_chipset_tag_t pct;
    pcmcia_chipset_handle_t pch;
    int window;
    struct pcmcia_mem_handle pcmh;
    struct pcmcia_tuple tuple;
    int longlink_present;
    int longlink_common;
    u_long longlink_addr;
    int mfc_count;
    int mfc_index;
    struct {
	int common;
	u_long addr;
    } mfc[256/5];
    int ret;

    ret = 0;

    pct = sc->pct;
    pch = sc->pch;

    /* allocate some memory */

    if (pcmcia_chip_mem_alloc(pct, pch, PCMCIA_CIS_SIZE, &pcmh)) {
#ifdef DIAGNOSTIC
	printf("%s: can't alloc memory to read attributes\n",
	       sc->dev.dv_xname);
#endif
	return(1);
    }

    tuple.memt = pcmh.memt;
    tuple.memh = pcmh.memh;

    /* initialize state for the primary tuple chain */
    if (pcmcia_chip_mem_map(pct, pch, PCMCIA_MEM_ATTR, 0,
			    PCMCIA_CIS_SIZE, &pcmh, &tuple.ptr, &window)) {
	pcmcia_chip_mem_free(pct, pch, &pcmh);
#ifdef DIAGNOSTIC
	printf("%s: can't map memory to read attributes\n", sc->dev.dv_xname);
#endif
	return(1);
    }

    DPRINTF(("cis mem map %x\n", (unsigned int) tuple.memh));

    tuple.mult = 2;

    longlink_present = 1;
    longlink_common = 1;
    longlink_addr = 0;

    mfc_count = 0;
    mfc_index = 0;

    DPRINTF(("%s: CIS tuple chain:\n", sc->dev.dv_xname));

    while (1) {
	while (1) {
	    /* get the tuple code */

	    tuple.code = pcmcia_cis_read_1(&tuple, tuple.ptr);

	    /* two special-case tuples */

	    if (tuple.code == 0) {
		DPRINTF(("CISTPL_NONE\n 00\n"));
		tuple.ptr++;
		continue;
	    } else if (tuple.code == 0xff) {
		DPRINTF(("CISTPL_END\n ff\n"));
		tuple.ptr++;
		break;
	    }

	    /* now all the normal tuples */

	    tuple.length = pcmcia_cis_read_1(&tuple, tuple.ptr+1);
	    switch (tuple.code) {
	    case PCMCIA_CISTPL_LONGLINK_A:
	    case PCMCIA_CISTPL_LONGLINK_C:
		if (tuple.length < 4) {
		    DPRINTF(("CISTPL_LONGLINK_%s too short %d\n",
			     longlink_common?"C":"A", tuple.length));
		    break;
		}

		longlink_present = 1;
		longlink_common = (tuple.code==PCMCIA_CISTPL_LONGLINK_C)?1:0;
		longlink_addr = pcmcia_tuple_read_4(&tuple, 0);
		DPRINTF(("CISTPL_LONGLINK_%s %lx\n", longlink_common?"C":"A",
			 longlink_addr));
		break;
	    case PCMCIA_CISTPL_NO_LINK:
		longlink_present = 0;
		DPRINTF(("CISTPL_NO_LINK\n"));
		break;
	    case PCMCIA_CISTPL_CHECKSUM:
		if (tuple.length < 5) {
		    DPRINTF(("CISTPL_CHECKSUM too short %d\n", tuple.length));
		    break;
		}
		{
		    int16_t offset;
		    u_long addr, length;
		    u_int cksum, sum;
		    int i;

		    *((u_int16_t *) &offset) = pcmcia_tuple_read_2(&tuple, 0);
		    length = pcmcia_tuple_read_2(&tuple, 2);
		    cksum = pcmcia_tuple_read_1(&tuple, 4);

		    addr = tuple.ptr+offset;

		    DPRINTF(("CISTPL_CHECKSUM addr=%lx len=%lx cksum=%x",
			     addr, length, cksum));

		    /* XXX do more work to deal with distant regions */
		    if ((addr >= PCMCIA_CIS_SIZE) ||
			((addr+length) < 0) ||
			((addr+length) >= PCMCIA_CIS_SIZE)) {
			DPRINTF((" skipped, too distant\n"));
			break;
		    }

		    sum = 0;
		    for (i=0; i<length; i++)
			sum += bus_space_read_1(tuple.memt, tuple.memh,
						addr+tuple.mult*i);
		    if (cksum != (sum & 0xff)) {
			DPRINTF((" failed sum=%x\n", sum));
			printf("%s: CIS checksum failed\n", sc->dev.dv_xname);
#if 0	/* XXX some working cards have bad checksums!! */
			ret = 1;
#endif
		    } else {
			DPRINTF((" ok\n"));
		    }
		}
		break;
	    case PCMCIA_CISTPL_LONGLINK_MFC:
		if (tuple.length < 1) {
		    DPRINTF(("CISTPL_LONGLINK_MFC too short %d\n",
			     tuple.length));
		    break;
		}
		/* this is kind of ad hoc, as I don't have any
		   real documentation */
		{
		    int i;
    
		    mfc_count = pcmcia_tuple_read_1(&tuple, 0);
		    DPRINTF(("CISTPL_LONGLINK_MFC %d", mfc_count));
		    for (i=0; i<mfc_count; i++) {
			mfc[i].common = (pcmcia_tuple_read_1(&tuple, 1+5*i) ==
					 PCMCIA_MFC_MEM_COMMON)?1:0;
			mfc[i].addr = pcmcia_tuple_read_4(&tuple, 1+5*i+1);
			DPRINTF((" %s:%lx", mfc[i].common?"common":"attr",
				 mfc[i].addr));
		    }
		    DPRINTF(("\n"));
		}
		/* for LONGLINK_MFC, fall through to the function.  This
		   tuple has structural and semantic content. */
	    default:
		{
		    if ((*fct)(&tuple, arg)) {
			pcmcia_chip_mem_unmap(pct, pch, window);
			goto done;
		    }
		}
		break;
	    } /* switch */
#ifdef PCMCIACISDEBUG
	    /* print the tuple */
	    {
		int i;

		DPRINTF((" %02x %02x", tuple.code, tuple.length));

		for (i=0; i<tuple.length; i++) {
		    DPRINTF((" %02x", pcmcia_tuple_read_1(&tuple, i)));
		    if ((i%16)==13)
			DPRINTF(("\n"));
		}
		if ((i%16)!=14)
		    DPRINTF(("\n"));
	    }
#endif
	    /* skip to the next tuple */
	    tuple.ptr += 2+tuple.length;
	}
	/* the chain is done.  Clean up and move onto the next one, if
           any. */

	pcmcia_chip_mem_unmap(pct, pch, window);

	if (longlink_present) {
	    /* if the longlink is to attribute memory, then it is
	       unindexed.  That is, if the link value is 0x100, then the
	       actual memory address is 0x200.  This means that we need to
	       multiply by 2 before calling mem_map, and then divide the
	       resulting ptr by 2 after. */

	    if (!longlink_common)
		longlink_addr *= 2;

	    pcmcia_chip_mem_map(pct, pch, longlink_common ?
				PCMCIA_MEM_COMMON : PCMCIA_MEM_ATTR,
				longlink_addr, PCMCIA_CIS_SIZE,
				&pcmh, &tuple.ptr, &window);

	    if (!longlink_common)
		tuple.ptr /= 2;

	    DPRINTF(("cis mem map %x\n", (unsigned int) tuple.memh));

	    tuple.mult = longlink_common?1:2;
	    longlink_present = 0;
	    longlink_common = 1;
	    longlink_addr = 0;
	} else if (mfc_count && (mfc_index < mfc_count)) {
	    if (!mfc[mfc_index].common)
		mfc[mfc_index].addr *= 2;

	    pcmcia_chip_mem_map(pct, pch, mfc[mfc_index].common ?
				PCMCIA_MEM_COMMON : PCMCIA_MEM_ATTR,
				mfc[mfc_index].addr, PCMCIA_CIS_SIZE,
				&pcmh, &tuple.ptr, &window);

	    if (!mfc[mfc_index].common)
		tuple.ptr /= 2;

	    DPRINTF(("cis mem map %x\n", (unsigned int) tuple.memh));

	    /* set parse state, and point at the next one */

	    tuple.mult = mfc[mfc_index].common?1:2;

	    mfc_index++;
	} else {
	    goto done;
	}

	/* make sure that the link is valid */
	tuple.code = pcmcia_cis_read_1(&tuple, tuple.ptr);
	if (tuple.code != PCMCIA_CISTPL_LINKTARGET)
	    break;
	tuple.length = pcmcia_cis_read_1(&tuple, tuple.ptr+1);
	if (tuple.length < 3) {
	    DPRINTF(("CISTPL_LINKTARGET too short %d", tuple.length));
	    break;
	}
	if ((pcmcia_tuple_read_1(&tuple, 0) != 'C') ||
	    (pcmcia_tuple_read_1(&tuple, 1) != 'I') ||
	    (pcmcia_tuple_read_1(&tuple, 2) != 'S'))
	    break;

	tuple.ptr += 2+tuple.length;
    }

    pcmcia_chip_mem_unmap(pct, pch, window);

done:
    /* Last, free the allocated memory block */
    pcmcia_chip_mem_free(pct, pch, &pcmh);

    return(ret);
}

/* XXX this is incredibly verbose.  Not sure what trt is */

void
pcmcia_print_cis(sc)
     struct pcmcia_softc *sc;
{
    struct pcmcia_card *card = &sc->card;
    int i;
    struct pcmcia_function *pf;
    struct pcmcia_config_entry *cfe;

    printf("%s: CIS version ", sc->dev.dv_xname);
    if ((card->cis1_major == 4) && (card->cis1_minor == 1))
	printf("2.0 or 2.01\n");
    else if ((card->cis1_major == 5) && (card->cis1_minor == 0))
	printf("2.1\n");
    else
	printf("unknown (major=%d, minor=%d)\n",
	       card->cis1_major, card->cis1_minor);

    printf("%s: CIS info: ", sc->dev.dv_xname);
    for (i=0; i<4; i++) {
	if (!card->cis1_info[i])
	    break;
	if (i)
	    printf(", ");
	printf("%s", card->cis1_info[i]);
    }
    printf("\n");

    printf("%s: Manufacturer code 0x%x, product 0x%x\n",
	   sc->dev.dv_xname, card->manufacturer, card->product);

    for (pf = card->pf_head.sqh_first; pf; pf = pf->pf_list.sqe_next) {
	printf("%s: function %d: ", sc->dev.dv_xname, pf->number);

	switch (pf->function) {
	case 0: printf("multi-function"); break;
	case 1: printf("memory"); break;
	case 2: printf("serial port"); break;
	case 3: printf("parallel port"); break;
	case 4: printf("fixed disk"); break;
	case 5: printf("video adapter"); break;
	case 6: printf("network adapter"); break;
	case 7: printf("auto incrementing mass storage"); break;
	default: printf("unknown (%d)", pf->function); break;
	}

	printf(", ccr addr %lx mask %lx\n", pf->ccr_base, pf->ccr_mask);

	for (cfe = pf->cfe_head.sqh_first; cfe; cfe = cfe->cfe_list.sqe_next) {
	    printf("%s: function %d, config table entry %d: ",
		   sc->dev.dv_xname, pf->number, cfe->number);

	    switch (cfe->iftype) {
	    case PCMCIA_IFTYPE_MEMORY: printf("memory card"); break;
	    case PCMCIA_IFTYPE_IO: printf("I/O card"); break;
	    default: printf("card type unknown"); break;
	    }

	    printf("; irq mask %x", cfe->irqmask);

	    if (cfe->num_iospace) {
		printf("; iomask %lx, iospace", cfe->iomask);

		for (i=0; i<cfe->num_iospace; i++)
		    printf(" %lx%s%lx", cfe->iospace[i].start,
			   cfe->iospace[i].length?"-":"",
			   cfe->iospace[i].start+cfe->iospace[i].length-1);
	    }

	    if (cfe->num_memspace) {
		printf("; memspace");
	
		for (i=0; i<cfe->num_memspace; i++)
		    printf(" %lx%s%lx%s%lx", cfe->memspace[i].cardaddr,
			   cfe->memspace[i].length?"-":"",
			   cfe->memspace[i].cardaddr+cfe->memspace[i].length-1,
			   cfe->memspace[i].hostaddr?"@":"",
			   cfe->memspace[i].hostaddr);
	    }

	    if (cfe->maxtwins)
		printf("; maxtwins %d", cfe->maxtwins);

	    printf(";");

	    if (cfe->flags & PCMCIA_CFE_MWAIT_REQUIRED)
		printf(" mwait_required");
	    if (cfe->flags & PCMCIA_CFE_RDYBSY_ACTIVE)
		printf(" rdybsy_active");
	    if (cfe->flags & PCMCIA_CFE_WP_ACTIVE)
		printf(" wp_active");
	    if (cfe->flags & PCMCIA_CFE_BVD_ACTIVE)
		printf(" bvd_active");
	    if (cfe->flags & PCMCIA_CFE_IO8)
		printf(" io8");
	    if (cfe->flags & PCMCIA_CFE_IO16)
		printf(" io16");
	    if (cfe->flags & PCMCIA_CFE_IRQSHARE)
		printf(" irqshare");
	    if (cfe->flags & PCMCIA_CFE_IRQPULSE)
		printf(" irqpulse");
	    if (cfe->flags & PCMCIA_CFE_IRQLEVEL)
		printf(" irqlevel");
	    if (cfe->flags & PCMCIA_CFE_POWERDOWN)
		printf(" powerdown");
	    if (cfe->flags & PCMCIA_CFE_READONLY)
		printf(" readonly");
	    if (cfe->flags & PCMCIA_CFE_AUDIO)
		printf(" audio");

	    printf("\n");
	}
    }

    if (card->error)
	printf("%s: %d errors found while parsing CIS\n",
	   sc->dev.dv_xname, card->error);
}

int
pcmcia_parse_cis_tuple(tuple, arg)
     struct pcmcia_tuple *tuple;
     void *arg;
{
    /* most of these are educated guesses */
    static struct pcmcia_config_entry init_cfe = {
	-1, PCMCIA_CFE_RDYBSY_ACTIVE | PCMCIA_CFE_WP_ACTIVE |
	PCMCIA_CFE_BVD_ACTIVE, PCMCIA_IFTYPE_MEMORY,
    };

    struct cis_state *state = (struct cis_state *) arg;

    switch (tuple->code) {
    case PCMCIA_CISTPL_LONGLINK_MFC:
	/* this tuple's structure was dealt with in scan_cis.  deal
	   with the semantics here: remove any functions which have
	   already been made, clear state->pf, reset the counter */
	{
	    struct pcmcia_function *pf, *pfnext;

	    for (pf = state->card->pf_head.sqh_first; pf; pf = pfnext) {
		pfnext = pf->pf_list.sqe_next;
		free(pf, M_DEVBUF);
	    }

	    SIMPLEQ_INIT(&state->card->pf_head);

	    state->count = 0;
	    state->gotmfc = 1;
	    state->pf = NULL;
	}
        break;
#ifdef PCMCIACISDEBUG
    case PCMCIA_CISTPL_DEVICE:
    case PCMCIA_CISTPL_DEVICE_A:
	{
	    u_int reg, dtype, dspeed;

	    reg = pcmcia_tuple_read_1(tuple, 0);
	    dtype = reg & PCMCIA_DTYPE_MASK;
	    dspeed = reg & PCMCIA_DSPEED_MASK;

	    DPRINTF(("CISTPL_DEVICE%s type=",
		     (tuple->code == PCMCIA_CISTPL_DEVICE)?"":"_A"));
	    switch (dtype) {
	    case PCMCIA_DTYPE_NULL:
		DPRINTF(("null"));
		break;
	    case PCMCIA_DTYPE_ROM:
		DPRINTF(("rom"));
		break;
	    case PCMCIA_DTYPE_OTPROM:				
		DPRINTF(("otprom"));
		break;
	    case PCMCIA_DTYPE_EPROM:
		DPRINTF(("eprom"));
		break;
	    case PCMCIA_DTYPE_EEPROM:
		DPRINTF(("eeprom"));
		break;
	    case PCMCIA_DTYPE_FLASH:
		DPRINTF(("flash"));
		break;
	    case PCMCIA_DTYPE_SRAM:
		DPRINTF(("sram"));
		break;
	    case PCMCIA_DTYPE_DRAM:
		DPRINTF(("dram"));
		break;
	    case PCMCIA_DTYPE_FUNCSPEC:
		DPRINTF(("funcspec"));
		break;
	    case PCMCIA_DTYPE_EXTEND:
		DPRINTF(("extend"));
		break;
	    default:
		DPRINTF(("reserved"));
		break;
	    }
	    DPRINTF((" speed="));
	    switch (dspeed) {
	    case PCMCIA_DSPEED_NULL:
		DPRINTF(("null"));
		break;
	    case PCMCIA_DSPEED_250NS:
		DPRINTF(("250ns"));
		break;
	    case PCMCIA_DSPEED_200NS:
		DPRINTF(("200ns"));
		break;
	    case PCMCIA_DSPEED_150NS:
		DPRINTF(("150ns"));
		break;
	    case PCMCIA_DSPEED_100NS:
		DPRINTF(("100ns"));
		break;
	    case PCMCIA_DSPEED_EXT:
		DPRINTF(("ext"));
		break;
	    default:
		DPRINTF(("reserved"));
		break;
	    }
	}
        DPRINTF(("\n"));
        break;
#endif
    case PCMCIA_CISTPL_VERS_1:
	if (tuple->length < 6) {
	    DPRINTF(("CISTPL_VERS_1 too short %d\n", tuple->length));
	    break;
	}
	{
	    int start, i, ch, count;

	    state->card->cis1_major = pcmcia_tuple_read_1(tuple, 0);
	    state->card->cis1_minor = pcmcia_tuple_read_1(tuple, 1);

	    for (count = 0, start = 0, i = 0;
		 (count < 4) && ((i+4)<256);
		 i++) {
		ch = pcmcia_tuple_read_1(tuple, 2+i);
		if (ch == 0xff)
		    break;
		state->card->cis1_info_buf[i] = ch;
		if (ch == 0) {
		    state->card->cis1_info[count] =
			state->card->cis1_info_buf+start;
		    start = i+1;
		    count++;
		}
	    }
	    DPRINTF(("CISTPL_VERS_1\n"));
	}
	break;
    case PCMCIA_CISTPL_MANFID:
	if (tuple->length < 4) {
	    DPRINTF(("CISTPL_MANFID too short %d\n", tuple->length));
	    break;
	}
	state->card->manufacturer = pcmcia_tuple_read_2(tuple, 0);
	state->card->product = pcmcia_tuple_read_2(tuple, 2);
	DPRINTF(("CISTPL_MANFID\n"));
	break;
    case PCMCIA_CISTPL_FUNCID:
	if (tuple->length < 1) {
	    DPRINTF(("CISTPL_FUNCID too short %d\n", tuple->length));
	    break;
	}

	if ((state->pf == NULL) || state->gotmfc) {
	    state->pf = malloc(sizeof(*state->pf), M_DEVBUF, M_NOWAIT);
	    state->pf->number = state->count++;
	    state->pf->last_config_index = -1;
	    SIMPLEQ_INIT(&state->pf->cfe_head);

	    SIMPLEQ_INSERT_TAIL(&state->card->pf_head, state->pf, pf_list);
	}

	state->pf->function = pcmcia_tuple_read_1(tuple, 0);

	DPRINTF(("CISTPL_FUNCID\n"));
	break;
    case PCMCIA_CISTPL_CONFIG:
	if (tuple->length < 3) {
	    DPRINTF(("CISTPL_CONFIG too short %d\n", tuple->length));
	    break;
	}
	{
	    u_int reg, rasz, rmsz, rfsz;
	    int i;

	    reg = pcmcia_tuple_read_1(tuple, 0);
	    rasz = 1 + ((reg & PCMCIA_TPCC_RASZ_MASK) >>
			PCMCIA_TPCC_RASZ_SHIFT);
	    rmsz = 1 + ((reg & PCMCIA_TPCC_RMSZ_MASK) >>
			PCMCIA_TPCC_RMSZ_SHIFT);
	    rfsz = ((reg & PCMCIA_TPCC_RFSZ_MASK) >>
		    PCMCIA_TPCC_RFSZ_SHIFT);

	    if (tuple->length < (rasz + rmsz + rfsz)) {
		DPRINTF(("CISTPL_CONFIG (%d,%d,%d) too short %d\n", 
			 rasz, rmsz, rfsz, tuple->length));
		break;
	    }

	    if (state->pf == NULL) {
		state->pf = malloc(sizeof(*state->pf), M_DEVBUF, M_NOWAIT);
		state->pf->number = state->count++;
		state->pf->last_config_index = -1;
		SIMPLEQ_INIT(&state->pf->cfe_head);

		SIMPLEQ_INSERT_TAIL(&state->card->pf_head, state->pf, pf_list);

		state->pf->function = 0;
	    }

	    state->pf->last_config_index = pcmcia_tuple_read_1(tuple, 1);

	    state->pf->ccr_base = 0;
	    for (i=0; i<rasz; i++)
		state->pf->ccr_base |=
		    ((pcmcia_tuple_read_1(tuple, 2+i)) << (i*8));

	    state->pf->ccr_mask = 0;
	    for (i=0; i<rmsz; i++)
		state->pf->ccr_mask |= 
		    ((pcmcia_tuple_read_1(tuple, 2+rasz+i)) << (i*8));

	    /* skip the reserved area and subtuples */

	    /* reset the default cfe for each cfe list */
	    state->temp_cfe = init_cfe;
	    state->default_cfe = &state->temp_cfe;
	}
	DPRINTF(("CISTPL_CONFIG\n"));
	break;
    case PCMCIA_CISTPL_CFTABLE_ENTRY:
	{
	    int idx, i, j;
	    u_int reg, reg2;
	    u_int intface, def, num;
	    u_int power, timing, iospace, irq, memspace, misc;
	    struct pcmcia_config_entry *cfe;

	    idx = 0;

	    reg = pcmcia_tuple_read_1(tuple, idx);
	    idx++;
	    intface = reg & PCMCIA_TPCE_INDX_INTFACE;
	    def = reg & PCMCIA_TPCE_INDX_DEFAULT;
	    num = reg & PCMCIA_TPCE_INDX_NUM_MASK;

	    /* 
	     * this is a little messy.  Some cards have only a cfentry
	     * with the default bit set.  So, as we go through the
	     * list, we add new indexes to the queue, and keep a
	     * pointer to the last one with the default bit set.  if
	     * we see a record with the same index, as the default, we
	     * stash the default and replace the queue entry.
	     * otherwise, we just add new entries to the queue,
	     * pointing the default ptr at them if the default bit is
	     * set.  if we get to the end with the default pointer
	     * pointing at a record which hasn't had a matching index,
	     * that's ok; it just becomes a cfentry like any other.
	     */

	    /* if the index in the cis differs from the default cis,
	       create new entry in the queue and start it with the
	       current default */
	    if (num != state->default_cfe->number) {
		cfe = (struct pcmcia_config_entry *)
		    malloc(sizeof(*cfe), M_DEVBUF, M_NOWAIT);

		*cfe = *state->default_cfe;

		SIMPLEQ_INSERT_TAIL(&state->pf->cfe_head, cfe, cfe_list);

		cfe->number = num;

		/* if the default bit is set in the cis, then point
		   the new default at whatever is being filled in */
		if (def)
		    state->default_cfe = cfe;
	    } else {
		/* 
		 * the cis index matches the default index, fill in
		 * the default cfentry.  It is assumed that the
		 * cfdefault index is in the queue.  For it to be
		 * otherwise, the cis index would have to be -1
		 * (initial condition) which is not possible, or there
		 * would have to be a preceding cis entry which had
		 * the same cis index and had the default bit unset.
		 * Neither condition should happen.  If it does, this
		 * cfentry is lost (written into temp space), which is
		 * an acceptable failure mode.
		 */

		cfe = state->default_cfe;

		/* if the cis entry does not have the default bit set,
		   copy the default out of the way first. */
		if (!def) {
		    state->temp_cfe = *state->default_cfe;
		    state->default_cfe = &state->temp_cfe;
		}
	    }

	    if (intface) {
		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;
		if (reg & PCMCIA_TPCE_IF_MWAIT)
		    cfe->flags |= PCMCIA_CFE_MWAIT_REQUIRED;
		if (reg & PCMCIA_TPCE_IF_RDYBSY)
		    cfe->flags |= PCMCIA_CFE_RDYBSY_ACTIVE;
		if (reg & PCMCIA_TPCE_IF_WP)
		    cfe->flags |= PCMCIA_CFE_WP_ACTIVE;
		if (reg & PCMCIA_TPCE_IF_BVD)
		    cfe->flags |= PCMCIA_CFE_BVD_ACTIVE;
		cfe->iftype = reg & PCMCIA_TPCE_IF_IFTYPE;
	    }

	    reg = pcmcia_tuple_read_1(tuple, idx);
	    idx++;

	    power = reg & PCMCIA_TPCE_FS_POWER_MASK;
	    timing = reg & PCMCIA_TPCE_FS_TIMING;
	    iospace = reg & PCMCIA_TPCE_FS_IOSPACE;
	    irq = reg & PCMCIA_TPCE_FS_IRQ;
	    memspace = reg & PCMCIA_TPCE_FS_MEMSPACE_MASK;
	    misc = reg & PCMCIA_TPCE_FS_MISC;

	    if (power) {
		/* skip over power, don't save */
		/* for each parameter selection byte */
		for (i=0; i<power; i++) {
		    reg = pcmcia_tuple_read_1(tuple, idx);
		    idx++;
		    /* for each bit */
		    for (j=0; j<7; j++) {
			/* if the bit is set */
			if ((reg >> j) & 0x01) {
			    /* skip over bytes */
			    do {
				reg2 = pcmcia_tuple_read_1(tuple, idx);
				idx++;
				/* until non-extension byte */
			    } while (reg2 & 0x80);
			}
		    }
		}
	    }

	    if (timing) {
		/* skip over timing, don't save */
		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;

		if ((reg & PCMCIA_TPCE_TD_RESERVED_MASK) != 
		    PCMCIA_TPCE_TD_RESERVED_MASK)
		    idx++;
		if ((reg & PCMCIA_TPCE_TD_RDYBSY_MASK) != 
		    PCMCIA_TPCE_TD_RDYBSY_MASK)
		    idx++;
		if ((reg & PCMCIA_TPCE_TD_WAIT_MASK) != 
		    PCMCIA_TPCE_TD_WAIT_MASK)
		    idx++;
	    }

	    if (iospace) {
		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;

		if (reg & PCMCIA_TPCE_IO_BUSWIDTH_8BIT)
		    cfe->flags |= PCMCIA_CFE_IO8;
		if (reg & PCMCIA_TPCE_IO_BUSWIDTH_16BIT)
		    cfe->flags |= PCMCIA_CFE_IO16;
		cfe->iomask = reg & PCMCIA_TPCE_IO_IOADDRLINES_MASK;

		if (reg & PCMCIA_TPCE_IO_HASRANGE) {
		    reg = pcmcia_tuple_read_1(tuple, idx);
		    idx++;

		    cfe->num_iospace =
			1 + (reg & PCMCIA_TPCE_IO_RANGE_COUNT);

		    if (cfe->num_iospace >
			(sizeof(cfe->iospace)/
			 sizeof(cfe->iospace[0]))) {
			DPRINTF(("too many io spaces %d",
				 cfe->num_iospace));
			state->card->error++;
			break;
		    }

		    for (i=0; i < cfe->num_iospace; i++) {
			switch (reg &
				PCMCIA_TPCE_IO_RANGE_ADDRSIZE_MASK) {
			case PCMCIA_TPCE_IO_RANGE_ADDRSIZE_ONE:
			    cfe->iospace[i].start = 
				pcmcia_tuple_read_1(tuple, idx);
			    idx++;
			    break;
			case PCMCIA_TPCE_IO_RANGE_ADDRSIZE_TWO:
			    cfe->iospace[i].start =
				pcmcia_tuple_read_2(tuple, idx);
			    idx+=2;
			    break;
			case PCMCIA_TPCE_IO_RANGE_ADDRSIZE_FOUR:
			    cfe->iospace[i].start =
				pcmcia_tuple_read_4(tuple, idx);
			    idx+=4;
			    break;
			}
			switch (reg &
				PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_MASK) {
			case PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_ONE:
			    cfe->iospace[i].length =
				pcmcia_tuple_read_1(tuple, idx);
			    idx++;
			    break;
			case PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_TWO:
			    cfe->iospace[i].length =
				pcmcia_tuple_read_2(tuple, idx);
			    idx+=2;
			    break;
			case PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_FOUR:
			    cfe->iospace[i].length =
				pcmcia_tuple_read_4(tuple, idx);
			    idx+=4;
			    break;
			}
			cfe->iospace[i].length++;
		    }
		} else {
		    cfe->num_iospace = 1;
		    cfe->iospace[0].start = 0;
		    cfe->iospace[0].length = (1<<cfe->iomask);
		}
	    }

	    if (irq) {
		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;

		if (reg & PCMCIA_TPCE_IR_SHARE)
		    cfe->flags |= PCMCIA_CFE_IRQSHARE;
		if (reg & PCMCIA_TPCE_IR_PULSE)
		    cfe->flags |= PCMCIA_CFE_IRQPULSE;
		if (reg & PCMCIA_TPCE_IR_LEVEL)
		    cfe->flags |= PCMCIA_CFE_IRQLEVEL;

		if (reg & PCMCIA_TPCE_IR_HASMASK) {
		    /* it's legal to ignore the special-interrupt
		       bits, so I will */

		    cfe->irqmask = pcmcia_tuple_read_2(tuple, idx);
		    idx+=2;
		} else {
		    cfe->irqmask = (1 << (reg & PCMCIA_TPCE_IR_IRQ));
		}
	    }

	    if (memspace) {
		if (memspace == PCMCIA_TPCE_FS_MEMSPACE_LENGTH) {
		    cfe->num_memspace = 1;
		    cfe->memspace[0].length =
			256*pcmcia_tuple_read_2(tuple, idx);
		    idx+=2;
		    cfe->memspace[0].cardaddr = 0;
		    cfe->memspace[0].hostaddr = 0;
		} else if (memspace ==
			   PCMCIA_TPCE_FS_MEMSPACE_LENGTHADDR) {
		    cfe->num_memspace = 1;
		    cfe->memspace[0].length =
			256*pcmcia_tuple_read_2(tuple, idx);
		    idx+=2;
		    cfe->memspace[0].cardaddr =
			256*pcmcia_tuple_read_2(tuple, idx);
		    idx+=2;
		    cfe->memspace[0].hostaddr = 0;
		} else {
		    int lengthsize;
		    int cardaddrsize;
		    int hostaddrsize;

		    reg = pcmcia_tuple_read_1(tuple, idx);
		    idx++;

		    cfe->num_memspace = reg & PCMCIA_TPCE_MS_COUNT;

		    if (cfe->num_memspace >
			(sizeof(cfe->memspace)/
			 sizeof(cfe->memspace[0]))) {
			DPRINTF(("too many mem spaces %d",
				 cfe->num_memspace));
			state->card->error++;
			break;
		    }

		    lengthsize =
			((reg & PCMCIA_TPCE_MS_LENGTH_SIZE_MASK) >>
			 PCMCIA_TPCE_MS_LENGTH_SIZE_SHIFT);
		    cardaddrsize =
			((reg & PCMCIA_TPCE_MS_CARDADDR_SIZE_MASK) >>
			 PCMCIA_TPCE_MS_CARDADDR_SIZE_SHIFT);
		    hostaddrsize =
			(reg & PCMCIA_TPCE_MS_HOSTADDR)?cardaddrsize:0;

		    if (lengthsize == 0) {
			DPRINTF(("cfe memspace lengthsize == 0"));
			state->card->error++;
		    }

		    for (i=0; i<cfe->num_memspace; i++) {
			if (lengthsize) {
			    cfe->memspace[i].length =
				256*pcmcia_tuple_read_n(tuple, lengthsize,
							idx);
			    idx += lengthsize;
			} else {
			    cfe->memspace[i].length = 0;
			}
			if (cfe->memspace[i].length == 0) {
			    DPRINTF(("cfe->memspace[%d].length == 0",
				     i));
			    state->card->error++;
			}
			if (cardaddrsize) {
			    cfe->memspace[i].cardaddr =
				256*pcmcia_tuple_read_n(tuple, cardaddrsize,
							idx);
			    idx += cardaddrsize;
			} else {
			    cfe->memspace[i].cardaddr = 0;
			}
			if (hostaddrsize) {
			    cfe->memspace[i].hostaddr =
				256*pcmcia_tuple_read_n(tuple, hostaddrsize,
							idx);
			    idx += hostaddrsize;
			} else {
			    cfe->memspace[i].hostaddr = 0;
			}
		    }
		}
	    }

	    if (misc) {
		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;

		if (reg & PCMCIA_TPCE_MI_PWRDOWN)
		    cfe->flags = PCMCIA_CFE_POWERDOWN;
		if (reg & PCMCIA_TPCE_MI_READONLY)
		    cfe->flags = PCMCIA_CFE_READONLY;
		if (reg & PCMCIA_TPCE_MI_AUDIO)
		    cfe->flags = PCMCIA_CFE_AUDIO;
		cfe->maxtwins = reg & PCMCIA_TPCE_MI_MAXTWINS;

		while (reg & PCMCIA_TPCE_MI_EXT) {
		    reg = pcmcia_tuple_read_1(tuple, idx);
		    idx++;
		}
	    }
	    /* skip all the subtuples */
	}
        DPRINTF(("CISTPL_CFTABLE_ENTRY\n"));
	break;
    default:
	DPRINTF(("unhandled CISTPL %x\n", tuple->code));
	break;
    }

    return(0);
}

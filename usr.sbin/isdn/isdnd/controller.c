/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - controller state support routines
 *	----------------------------------------------
 *
 *	$Id: controller.c,v 1.4.2.1 2004/07/23 15:03:58 tron Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Oct  9 14:37:34 2000]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

static int
init_controller_state(int controller, const char *devname, const char *cardname, int tei);

/*
 * add a single controller
 */
void
init_new_controller(int bri)
{
	msg_ctrl_info_req_t mcir;

	memset(&mcir, 0, sizeof mcir);
	mcir.controller = bri;
		
	if((ioctl(isdnfd, I4B_CTRL_INFO_REQ, &mcir)) < 0)
		return;

	if((init_controller_state(bri, mcir.devname, mcir.cardname, mcir.tei)) == ERROR)
	{
		logit(LL_ERR, "init_new_controller: init_controller_state for controller %d failed", bri);
		do_exit(1);
	}
}

/*---------------------------------------------------------------------------*
 *	init controller state array
 *---------------------------------------------------------------------------*/
void
init_controller(void)
{
	int i;
	int max = 0;
	msg_ctrl_info_req_t mcir;
	
	remove_all_ctrl_state();
	for(i=0; i <= max; i++)
	{
		mcir.controller = i;
		
		if((ioctl(isdnfd, I4B_CTRL_INFO_REQ, &mcir)) < 0)
			continue;

		max = mcir.maxbri;

		/* init controller tab */

		if((init_controller_state(i, mcir.devname, mcir.cardname, mcir.tei)) == ERROR)
		{
			logit(LL_ERR, "init_controller: init_controller_state for controller %d failed", i);
			do_exit(1);
		}
	}
	DBGL(DL_RCCF, (logit(LL_DBG, "init_controller: found %d ISDN controller(s)", max)));
}

/*--------------------------------------------------------------------------*
 *	init controller state table entry
 *--------------------------------------------------------------------------*/
static int
init_controller_state(int controller, const char *devname, const char *cardname, int tei)
{
	struct isdn_ctrl_state *ctrl;

	ctrl = malloc(sizeof *ctrl);
	if (ctrl == NULL) {
		logit(LL_ERR, "init_controller_state: out of memory");
		return(ERROR);
	}
	
	/* init controller state entry */
		
	memset(ctrl, 0, sizeof *ctrl);
	strncpy(ctrl->device_name,
	    devname, 
	    sizeof(ctrl->device_name)-1);
	strncpy(ctrl->controller,
	    cardname, 
	    sizeof(ctrl->controller)-1);
	ctrl->bri = controller;
	ctrl->protocol = PROTOCOL_DSS1;
	ctrl->state = CTRL_UP;
	ctrl->stateb1 = CHAN_IDLE;
	ctrl->stateb2 = CHAN_IDLE;
	ctrl->freechans = MAX_CHANCTRL;
	ctrl->tei = tei;
	ctrl->l1stat = LAYER_IDLE;
	ctrl->l2stat = LAYER_IDLE;
	DBGL(DL_RCCF, (logit(LL_DBG, "init_controller_state: controller %d (%s) is %s",
	   controller, devname, cardname)));

	/* add to list */
	add_ctrl_state(ctrl);

	return(GOOD);
}	

/*--------------------------------------------------------------------------*
 *	init active controller
 *--------------------------------------------------------------------------*/
void
init_active_controller(void)
{
/* XXX - replace by something usefull */
#if 0
	int ret;
	int unit = 0;
	int controller;
	char cmdbuf[MAXPATHLEN+128];

	for(controller = 0; controller < ncontroller; controller++)
	{
		if(isdn_ctrl_tab[controller].ctrl_type == CTRL_TINADD)
		{
			DBGL(DL_RCCF, (logit(LL_DBG, "init_active_controller, tina-dd %d: executing [%s %d]", unit, tinainitprog, unit)));
			
			snprintf(cmdbuf, sizeof(cmdbuf), "%s %d", tinainitprog, unit);

			if((ret = system(cmdbuf)) != 0)
			{
				logit(LL_ERR, "init_active_controller, tina-dd %d: %s returned %d!", unit, tinainitprog, ret);
				do_exit(1);
			}
		}
	}
#endif
}	

void
init_single_controller_protocol ( struct isdn_ctrl_state *ctrl )
{
	msg_prot_ind_t mpi;

	memset(&mpi, 0, sizeof mpi);
	mpi.controller = ctrl->bri;
	mpi.protocol = ctrl->protocol;
	
	if((ioctl(isdnfd, I4B_PROT_IND, &mpi)) < 0)
	{
		logit(LL_ERR, "init_single_controller_protocol: ioctl I4B_PROT_IND failed: %s", strerror(errno));
		do_exit(1);
	}
}

/*--------------------------------------------------------------------------*
 *	init controller D-channel ISDN protocol
 *--------------------------------------------------------------------------*/
void
init_controller_protocol(void)
{
	struct isdn_ctrl_state *ctrl;

	for (ctrl = get_first_ctrl_state(); ctrl; ctrl = NEXT_CTRL(ctrl))
		init_single_controller_protocol(ctrl);
}

/*--------------------------------------------------------------------------*
 *	set controller state to UP/DOWN
 *--------------------------------------------------------------------------*/
int
set_controller_state(struct isdn_ctrl_state *ctrl, int state)
{
	if (ctrl == NULL) {
		logit(LL_ERR, "set_controller_state: invalid controller");
		return(ERROR);
	}

	if (state == CTRL_UP) {
		ctrl->state = CTRL_UP;
		DBGL(DL_CNST, (logit(LL_DBG, "set_controller_state: controller [%d] set UP!", ctrl->bri)));
	}
	else if (state == CTRL_DOWN)
	{
		ctrl->state = CTRL_DOWN;
		DBGL(DL_CNST, (logit(LL_DBG, "set_controller_state: controller [%d] set DOWN!", ctrl->bri)));
	}
	else
	{
		logit(LL_ERR, "set_controller_state: invalid controller state [%d]!", state);
		return(ERROR);
	}
	return(GOOD);
}		
	
/*--------------------------------------------------------------------------*
 *	get controller state
 *--------------------------------------------------------------------------*/
int
get_controller_state(struct isdn_ctrl_state *ctrl)
{
	if (ctrl == NULL) {
		logit(LL_ERR, "set_controller_state: invalid controller");
		return(ERROR);
	}
	return (ctrl->state);
}		

/*--------------------------------------------------------------------------*
 *	decrement number of free channels for controller
 *--------------------------------------------------------------------------*/
int
decr_free_channels(struct isdn_ctrl_state *ctrl)
{
	if (ctrl == NULL) {
		logit(LL_ERR, "decr_free_channels: invalid controller!");
		return(ERROR);
	}
	if (ctrl->freechans > 0)
	{
		ctrl->freechans--;
		DBGL(DL_CNST, (logit(LL_DBG, "decr_free_channels: ctrl %d, now %d chan free", ctrl->bri, ctrl->freechans)));
		return(GOOD);
	}
	else
	{
		logit(LL_ERR, "decr_free_channels: controller [%d] already 0 free chans!", ctrl->bri);
		return(ERROR);
	}
}		
	
/*--------------------------------------------------------------------------*
 *	increment number of free channels for controller
 *--------------------------------------------------------------------------*/
int
incr_free_channels(struct isdn_ctrl_state *ctrl)
{
	if (ctrl == NULL) {
		logit(LL_ERR, "incr_free_channels: invalid controller!");
		return(ERROR);
	}
	if (ctrl->freechans < MAX_CHANCTRL)
	{
		ctrl->freechans++;
		DBGL(DL_CNST, (logit(LL_DBG, "incr_free_channels: ctrl %d, now %d chan free", ctrl->bri, ctrl->freechans)));
		return(GOOD);
	}
	else
	{
		logit(LL_ERR, "incr_free_channels: controller [%d] already 2 free chans!", ctrl->bri);
		return(ERROR);
	}
}		
	
/*--------------------------------------------------------------------------*
 *	get number of free channels for controller
 *--------------------------------------------------------------------------*/
int
get_free_channels(struct isdn_ctrl_state *ctrl)
{
	if (ctrl == NULL) {
		logit(LL_ERR, "get_free_channels: invalid controller!");
		return(ERROR);
	}
	DBGL(DL_CNST, (logit(LL_DBG, "get_free_channels: ctrl %d, %d chan free", ctrl->bri, ctrl->freechans)));
	return (ctrl->freechans);
}		
	
/*--------------------------------------------------------------------------*
 *	set channel state to busy
 *--------------------------------------------------------------------------*/
int
set_channel_busy(struct isdn_ctrl_state *ctrl, int channel)
{
	if (ctrl == NULL) {
		logit(LL_ERR, "set_channel_busy: invalid controller");
		return(ERROR);
	}
		
	switch(channel)
	{
		case CHAN_B1:
			if (ctrl->stateb1 == CHAN_RUN) {
				DBGL(DL_CNST, (logit(LL_DBG, "set_channel_busy: controller [%d] channel B1 already busy!", ctrl->bri)));
			}
			else
			{
				ctrl->stateb1 = CHAN_RUN;
				DBGL(DL_CNST, (logit(LL_DBG, "set_channel_busy: controller [%d] channel B1 set to BUSY!", ctrl->bri)));
			}
			break;

		case CHAN_B2:
			if (ctrl->stateb2 == CHAN_RUN)
			{
				DBGL(DL_CNST, (logit(LL_DBG, "set_channel_busy: controller [%d] channel B2 already busy!", ctrl->bri)));
			}
			else
			{
				ctrl->stateb2 = CHAN_RUN;
				DBGL(DL_CNST, (logit(LL_DBG, "set_channel_busy: controller [%d] channel B2 set to BUSY!", ctrl->bri)));
			}
			break;

		default:
			logit(LL_ERR, "set_channel_busy: controller [%d], invalid channel [%d]!", ctrl->bri, channel);
			return(ERROR);
			break;
	}
	return(GOOD);
}

/*--------------------------------------------------------------------------*
 *	set channel state to idle
 *--------------------------------------------------------------------------*/
int
set_channel_idle(struct isdn_ctrl_state *ctrl, int channel)
{
	if (ctrl == NULL) {
		logit(LL_ERR, "set_channel_idle: invalid controller");
		return(ERROR);
	}
		
	switch(channel)
	{
		case CHAN_B1:
			if (ctrl->stateb1 == CHAN_IDLE) {
				DBGL(DL_CNST, (logit(LL_DBG, "set_channel_idle: controller [%d] channel B1 already idle!", ctrl->bri)));
			} else {
				ctrl->stateb1 = CHAN_IDLE;
				DBGL(DL_CNST, (logit(LL_DBG, "set_channel_idle: controller [%d] channel B1 set to IDLE!", ctrl->bri)));
			}
			break;

		case CHAN_B2:
			if (ctrl->stateb2 == CHAN_IDLE) {
				DBGL(DL_CNST, (logit(LL_DBG, "set_channel_idle: controller [%d] channel B2 already idle!", ctrl->bri)));
			} else {
				ctrl->stateb2 = CHAN_IDLE;
				DBGL(DL_CNST, (logit(LL_DBG, "set_channel_idle: controller [%d] channel B2 set to IDLE!", ctrl->bri)));
			}
			break;

		default:
			DBGL(DL_CNST, (logit(LL_DBG, "set_channel_idle: controller [%d], invalid channel [%d]!", ctrl->bri, channel)));
			return(ERROR);
			break;
	}
	return(GOOD);
}

/*--------------------------------------------------------------------------*
 *	return channel state
 *--------------------------------------------------------------------------*/
int
ret_channel_state(struct isdn_ctrl_state *ctrl, int channel)
{
	if (ctrl == NULL) {
		logit(LL_ERR, "ret_channel_state: invalid controller!");
		return(ERROR);
	}
		
	switch(channel)
	{
		case CHAN_B1:
			return (ctrl->stateb1);
			break;

		case CHAN_B2:
			return (ctrl->stateb2);
			break;

		default:
			logit(LL_ERR, "ret_channel_state: controller [%d], invalid channel [%d]!", ctrl->bri, channel);
			return(ERROR);
			break;
	}
	return(ERROR);
}

/* EOF */

/*	$NetBSD: llc_var.h,v 1.12.18.5 2005/11/10 14:11:07 skrll Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Dirk Husemann and the Computer Science Department (IV) of
 * the University of Erlangen-Nuremberg, Germany.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)llc_var.h	8.2 (Berkeley) 2/9/95
 */

/*
 * Copyright (c) 1990, 1991, 1992
 *		Dirk Husemann, Computer Science Department IV,
 * 		University of Erlangen-Nuremberg, Germany.
 *
 * This code is derived from software contributed to Berkeley by
 * Dirk Husemann and the Computer Science Department (IV) of
 * the University of Erlangen-Nuremberg, Germany.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)llc_var.h	8.2 (Berkeley) 2/9/95
 */

struct llc;

#define NPAIDB_LINK	0

struct npaidbentry {
	union {
		/* MAC,DLSAP -> CONS */
		struct {
			struct llc_linkcb *NE_link;
			struct rtentry *NE_rt;
		} NE;
		/* SAP info for unconfigured incoming calls */
		struct {
			u_short SI_class;
#define LLC_CLASS_I	0x1
#define LLC_CLASS_II	0x3
#define LLC_CLASS_III	0x4				/* Future */
#define LLC_CLASS_IV	0x7				/* Future */
			u_short SI_window;
			u_short SI_trace;
			u_short SI_xchxid;
			void (*SI_input)
				__P((struct mbuf *,...));
			void *(*SI_ctlinput)
				__P((int,struct sockaddr *,void *));
		} SI;
	} NESIun;
};
#define np_link			NESIun.NE.NE_link
#define np_rt			NESIun.NE.NE_rt
#define si_class		NESIun.SI.SI_class
#define si_window		NESIun.SI.SI_window
#define si_trace		NESIun.SI.SI_trace
#define si_xchxid		NESIun.SI.SI_xchxid
#define si_input		NESIun.SI.SI_input
#define si_ctlinput		NESIun.SI.SI_ctlinput

#define NPDL_SAPNETMASK 0x7e

/*
 * Definitions for accessing bitfields/bitslices inside
 * LLC2 headers
 */
struct bitslice {
	unsigned int bs_mask;
	unsigned int bs_shift;
};


#define i_z		0
#define i_ns		1
#define i_pf		0
#define i_nr		1
#define s_oz		2
#define s_selector	3
#define s_pf		0
#define s_nr		1
#define u_bb		2
#define u_select_other	3
#define u_pf		4
#define u_select	5
#define f_vs		1
#define f_cr		0
#define f_vr		1
#define f_wxyzv		6

#define LLCGBITS(Arg,Index) \
	(((Arg) & llc_bitslice[(Index)].bs_mask) >> \
	 llc_bitslice[(Index)].bs_shift)
#define LLCSBITS(Arg,Index,Val) \
	(Arg) |= (((Val) << llc_bitslice[(Index)].bs_shift) & \
			    llc_bitslice[(Index)].bs_mask)
#define LLCCSBITS(Arg,Index,Val) \
	(Arg) = (((Val) << llc_bitslice[(Index)].bs_shift) & \
			   llc_bitslice[(Index)].bs_mask)

extern struct bitslice llc_bitslice[];

#define LLC_CMD		0
#define LLC_RSP		1
#define LLC_MAXCMDRSP	2

/*
 * LLC events --- These events may either be frames received from the
 *		  remote LLC DSAP,request from the network layer user,
 *		  timer events from llc_timer(),or diagnostic events from
 *		  llc_input().
 */

/* LLC frame types */
#define LLCFT_INFO			 0 * LLC_MAXCMDRSP
#define LLCFT_RR			 1 * LLC_MAXCMDRSP
#define LLCFT_RNR			 2 * LLC_MAXCMDRSP
#define LLCFT_REJ			 3 * LLC_MAXCMDRSP
#define LLCFT_DM			 4 * LLC_MAXCMDRSP
#define LLCFT_SABME			 5 * LLC_MAXCMDRSP
#define LLCFT_DISC			 6 * LLC_MAXCMDRSP
#define LLCFT_UA			 7 * LLC_MAXCMDRSP
#define LLCFT_FRMR			 8 * LLC_MAXCMDRSP
#define LLCFT_UI			 9 * LLC_MAXCMDRSP
#define LLCFT_XID			10 * LLC_MAXCMDRSP
#define LLCFT_TEST			11 * LLC_MAXCMDRSP

/* LLC2 timer events */
#define LLC_ACK_TIMER_EXPIRED		12 * LLC_MAXCMDRSP
#define LLC_P_TIMER_EXPIRED		13 * LLC_MAXCMDRSP
#define LLC_REJ_TIMER_EXPIRED		14 * LLC_MAXCMDRSP
#define LLC_BUSY_TIMER_EXPIRED		15 * LLC_MAXCMDRSP

/* LLC2 diagnostic events */
#define LLC_INVALID_NR			16 * LLC_MAXCMDRSP
#define LLC_INVALID_NS			17 * LLC_MAXCMDRSP
#define LLC_BAD_PDU			18 * LLC_MAXCMDRSP
#define LLC_LOCAL_BUSY_DETECTED		19 * LLC_MAXCMDRSP
#define LLC_LOCAL_BUSY_CLEARED		20 * LLC_MAXCMDRSP

/* Network layer user requests */
/*
 * NL_CONNECT_REQUEST --- The user has requested that a data link connection
 *			  be established with a remote LLC DSAP.
 */
#define NL_CONNECT_REQUEST		21 * LLC_MAXCMDRSP
/*
 * NL_CONNECT_RESPONSE --- The user has accepted the data link connection.
 */
#define NL_CONNECT_RESPONSE		22 * LLC_MAXCMDRSP
/*
 * NL_RESET_REQUEST --- The user has requested that the data link with the
 *			remote LLC DSAP be reset.
 */
#define NL_RESET_REQUEST		23 * LLC_MAXCMDRSP
/*
 * NL_RESET_RESPONSE --- The user has accepted the reset of the data link
 *			 connection.
 */
#define NL_RESET_RESPONSE		24 * LLC_MAXCMDRSP
/*
 * NL_DISCONNECT_REQUEST --- The user has requested that the data link
 *			     connection with remote LLC DSAP be terminated.
 */
#define NL_DISCONNECT_REQUEST		25 * LLC_MAXCMDRSP
/*
 * NL_DATA_REQUEST --- The user has requested that a data unit be sent ot the
 *		       remote LLC DSAP.
 */
#define NL_DATA_REQUEST			26 * LLC_MAXCMDRSP
/*
 * NL_INITIATE_PF_CYCLE --- The local LLC wants to initiate a P/F cycle.
 */
#define NL_INITIATE_PF_CYCLE		27 * LLC_MAXCMDRSP
/*
 * NL_LOCAL_BUSY_DETECTED --- The local entity has encountered a busy condition
 */
#define NL_LOCAL_BUSY_DETECTED		28 * LLC_MAXCMDRSP

#define LLCFT_NONE			255

/* return message from state handlers */

/*
 * LLC_CONNECT_INDICATION --- Inform the user that a connection has been
 *			      requested by a remote LLC SSAP.
 */
#define LLC_CONNECT_INDICATION	    1
/*
 * LLC_CONNECT_CONFIRM --- The connection service component indicates that the
 *			   remote network entity has accepted the connection.
 */
#define LLC_CONNECT_CONFIRM	    2
/*
 * LLC_DISCONNECT_INDICATION --- Inform the user that the remote network
 *				 entity has intiated disconnection of the data
 *				 link connection.
 */
#define LLC_DISCONNECT_INDICATION   3
/*
 * LLC_RESET_CONFIRM --- The connection service component indicates that the
 *			 remote network entity has accepted the reset.
 */
#define LLC_RESET_CONFIRM	    4
/*
 * LLC_RESET_INDICATION_REMOTE --- The remote network entity or remote peer
 *				   has initiated a reset of the data link
 *				   connection.
 */
#define LLC_RESET_INDICATION_REMOTE 5
/*
 * LLC_RESET_INDICATION_LOCAL --- The local LLC has determined that the data
 *				  link connection is in need of
 *				  reinitialization.
 */
#define LLC_RESET_INDICATION_LOCAL  6
/*
 * LLC_FRMR_RECEIVED --- The local connection service component has received a
 *			 FRMR response PDU.
 */
#define LLC_FRMR_RECEIVED	    7
/*
 * LLC_FRMR_SENT --- The local connection component has received an ivalid
 *		     PDU,and has sent a FRMR response PDU.
 */
#define LLC_FRMR_SENT		    8
/*
 * LLC_DATA_INDICATION --- The connection service component passes the data
 *			   unit from the received I PDU to the user.
 */
#define LLC_DATA_INDICATION	    9
/*
 * LLC_REMOTE_NOT_BUSY --- The remote LLC DSAP is no longer busy. The local
 *			   connection service component will now accept a
 *			   DATA_REQUEST.
 */
#define LLC_REMOTE_NOT_BUSY	    10
/*
 * LLC_REMOTE_BUSY --- The remote LLC DSAP is busy. The local connection
 *		       service component will not accept a DATA_REQUEST.
 */
#define LLC_REMOTE_BUSY		    11

/* Internal return code */
#define LLC_PASSITON		    255

#define INFORMATION_CONTROL	0x00
#define SUPERVISORY_CONTROL	0x02
#define UNUMBERED_CONTROL	0x03

/*
 * Other necessary definitions
 */

#define LLC_MAX_SEQUENCE    128
#define LLC_MAX_WINDOW	    127
#define LLC_WINDOW_SIZE	    7

/*
 * Don't we love this one? CCITT likes to suck on bits 8=)
 */
#define NLHDRSIZEGUESS	    3

/*
 * LLC control block
 */

struct llc_linkcb {
	struct llccb_q {
		struct llccb_q *q_forw;			/* admin chain */
		struct llccb_q *q_backw;
	} llcl_q;
	struct npaidbentry	*llcl_sapinfo;		/* SAP information */
	struct sockaddr_dl	llcl_addr;		/* link snpa address */
	struct rtentry		*llcl_nlrt;		/* layer 3 -> LLC */
	struct rtentry		*llcl_llrt;		/* LLC -> layer 3 */
	struct ifnet		*llcl_if;		/* our interface */
	caddr_t			llcl_nlnext;		/* cb for network layer */
	struct mbuf		*llcl_writeqh;		/* Write queue head */
	struct mbuf		*llcl_writeqt;		/* Write queue tail */
	struct mbuf		**llcl_output_buffers;
	short			llcl_timers[6];		/* timer array */
	long			llcl_timerflags;	/* flags signalling running timers */
	int			(*llcl_statehandler)
		__P((struct llc_linkcb *,struct llc *,int,int,int));
	int			llcl_P_flag;
	int			llcl_F_flag;
	int			llcl_S_flag;
	int			llcl_DATA_flag;
	int			llcl_REMOTE_BUSY_flag;
	int			llcl_DACTION_flag;	/* delayed action */
	int			llcl_retry;
	/*
	 * The following components deal --- in one way or the other ---
	 * with the LLC2 window. Indicated by either [L] or [W] is the
	 * domain of the specific component:
	 *
	 *	  [L]	 The domain is 0--LLC_MAX_WINDOW
	 *	  [W]	 The domain is 0--llcl_window
	 */
	short			llcl_vr;		/* next to receive [L] */
	short			llcl_vs;		/* next to send [L] */
	short			llcl_nr_received;	/* next frame to b ack'd [L] */
	short			llcl_freeslot;		/* next free slot [W] */
	short			llcl_projvs;		/* V(S) associated with freeslot */
	short			llcl_slotsfree;		/* free slots [W] */
	short			llcl_window;		/* window size */
	/*
	 * In llcl_frmrinfo we jot down the last frmr info field,which we
	 * need to do as we need to be able to resend it in the ERROR state.
	 */
	struct frmrinfo		llcl_frmrinfo;		/* last FRMR info field */
};
#define llcl_frmr_pdu0		llcl_frmrinfo.frmr_rej_pdu0
#define llcl_frmr_pdu1		llcl_frmrinfo.frmr_rej_pdu1
#define llcl_frmr_control	llcl_frmrinfo.frmr_control
#define llcl_frmr_control_ext	llcl_frmrinfo.frmr_control_ext
#define llcl_frmr_cause		llcl_frmrinfo.frmr_cause

#define LQNEXT(l)	(struct llc_linkcb *)((l)->llcl_q.q_forw)
#define LQEMPTY		(llccb_q.q_forw == &llccb_q)
#define LQFIRST		(struct llc_linkcb *)(llccb_q.q_forw)
#define LQVALID(l)	(!((struct llccb_q *)(l) == &llccb_q))

#define LLC_ENQUEUE(l,m) if ((l)->llcl_writeqh == NULL) { \
				(l)->llcl_writeqh = (m); \
				(l)->llcl_writeqt = (m); \
			} else { \
				(l)->llcl_writeqt->m_nextpkt = (m); \
				(l)->llcl_writeqt = (m); \
			}

#define LLC_DEQUEUE(l,m) if ((l)->llcl_writeqh == NULL) \
				(m) = NULL; \
			  else { \
				(m) = (l)->llcl_writeqh; \
				(l)->llcl_writeqh = (l)->llcl_writeqh->m_nextpkt; \
			}

#define LLC_SETFRAME(l,m) { \
				if ((l)->llcl_slotsfree > 0) { \
					(l)->llcl_slotsfree--; \
					(l)->llcl_output_buffers[(l)->llcl_freeslot] = (m); \
					(l)->llcl_freeslot = ((l)->llcl_freeslot+1) % (l)->llcl_window; \
					LLC_INC((l)->llcl_projvs); \
				} \
			   }

/*
 * handling of sockaddr_dl's
 */

#define LLADDRLEN(s)	((s)->sdl_alen + (s)->sdl_nlen)
#define LLSAPADDR(s)	((s)->sdl_data[LLADDRLEN(s)-1] & 0xff)
#define LLSAPLOC(s,if) ((s)->sdl_nlen + (if)->if_addrlen)

struct sdl_hdr {
	struct sockaddr_dl sdlhdr_dst;
	struct sockaddr_dl sdlhdr_src;
	long sdlhdr_len;
};

#define LLC_GETHDR(f,m) { \
	struct mbuf *_m = (struct mbuf *) (m); \
	if (_m) { \
		M_PREPEND(_m,LLC_ISFRAMELEN,M_DONTWAIT); \
		bzero(mtod(_m,caddr_t),LLC_ISFRAMELEN); \
	} else { \
		MGETHDR (_m,M_DONTWAIT,MT_HEADER); \
		if (_m != NULL) { \
			_m->m_pkthdr.len = _m->m_len = LLC_UFRAMELEN; \
			_m->m_next = _m->m_nextpkt = NULL; \
			bzero(mtod(_m,caddr_t),LLC_UFRAMELEN); \
		} else return; \
	} \
	(m) = _m; \
	(f) = mtod(m,struct llc *); \
}

#define LLC_NEWSTATE(l,LLCstate) \
	(l)->llcl_statehandler = __CONCAT(llc_state_,LLCstate)
#define LLC_STATEEQ(l,LLCstate) \
	((l)->llcl_statehandler == __CONCAT(llc_state_,LLCstate) ? 1 : 0)

#define LLC_ACK_SHIFT	   0
#define LLC_P_SHIFT	   1
#define LLC_BUSY_SHIFT	   2
#define LLC_REJ_SHIFT	   3
#define LLC_AGE_SHIFT	   4
#define LLC_DACTION_SHIFT  5

#define LLC_TIMER_NOTRUNNING	0
#define LLC_TIMER_RUNNING	1
#define LLC_TIMER_EXPIRED	2

#ifndef __CONCAT3
# if __STDC__
#  define __CONCAT3(a,b,c) a ## b ## c
# else
#  define __CONCAT3(a,b,c) a/**/b/**/c
# endif /* __STDC__ */
#endif

#define LLC_STARTTIMER(l,LLCtimer) { \
	(l)->llcl_timers[__CONCAT3(LLC_,LLCtimer,_SHIFT)] = \
		__CONCAT3(llc_,LLCtimer,_timer); \
	(l)->llcl_timerflags |= (1 << __CONCAT3(LLC_,LLCtimer,_SHIFT)); \
}
#define LLC_STOPTIMER(l,LLCtimer) { \
	(l)->llcl_timers[__CONCAT3(LLC_,LLCtimer,_SHIFT)] = 0; \
	(l)->llcl_timerflags &= ~(1<<__CONCAT3(LLC_,LLCtimer,_SHIFT)); \
}
#define LLC_AGETIMER(l,LLCtimer) \
	if ((l)->llcl_timers[__CONCAT3(LLC_,LLCtimer,_SHIFT)] > 0) \
		(l)->llcl_timers[__CONCAT3(LLC_,LLCtimer,_SHIFT)]--;

#define LLC_TIMERXPIRED(l,LLCtimer) \
	(((l)->llcl_timerflags & (1<<__CONCAT3(LLC_,LLCtimer,_SHIFT))) ? \
	 (((l)->llcl_timers[__CONCAT3(LLC_,LLCtimer,_SHIFT)] == 0 ) ? \
	  LLC_TIMER_EXPIRED : LLC_TIMER_RUNNING) : LLC_TIMER_NOTRUNNING)

#define FOR_ALL_LLC_TIMERS(t) \
	for ((t) = LLC_ACK_SHIFT; (t) < LLC_AGE_SHIFT; (t)++)

#define LLC_SETFLAG(l,LLCflag,v) (l)->__CONCAT3(llcl_,LLCflag,_flag) = (v)
#define LLC_GETFLAG(l,LLCflag) (l)->__CONCAT3(llcl_,LLCflag,_flag)

#define LLC_RESETCOUNTER(l) { \
	(l)->llcl_vs = (l)->llcl_vr = (l)->llcl_retry = 0; \
	llc_resetwindow((l)); \
}

/*
 * LLC2 macro definitions
 */


#define LLC_START_ACK_TIMER(l) LLC_STARTTIMER((l),ACK)
#define LLC_STOP_ACK_TIMER(l) LLC_STOPTIMER((l),ACK)
#define LLC_START_REJ_TIMER(l) LLC_STARTTIMER((l),REJ)
#define LLC_STOP_REJ_TIMER(l) LLC_STOPTIMER((l),REJ)
#define LLC_START_P_TIMER(l) { \
	LLC_STARTTIMER((l),P); \
	if (LLC_GETFLAG((l),P) == 0) \
		(l)->llcl_retry = 0; \
	LLC_SETFLAG((l),P,1); \
}
#define LLC_STOP_P_TIMER(l) { \
	LLC_STOPTIMER((l),P); \
	LLC_SETFLAG((l),P,0); \
}
#define LLC_STOP_ALL_TIMERS(l) { \
	LLC_STOPTIMER((l),ACK); \
	LLC_STOPTIMER((l),REJ); \
	LLC_STOPTIMER((l),BUSY); \
	LLC_STOPTIMER((l),P); \
}


#define LLC_INC(i) (i) = ((i)+1) % LLC_MAX_SEQUENCE

#define LLC_NR_VALID(l,nr) \
	((l)->llcl_vs < (l)->llcl_nr_received ? \
	 (((nr) >= (l)->llcl_nr_received) || \
	  ((nr) <= (l)->llcl_vs) ? 1 : 0) : \
	 (((nr) <= (l)->llcl_vs) && \
	  ((nr) >= (l)->llcl_nr_received) ? 1 : 0))

#define LLC_UPDATE_P_FLAG(l,cr,pf) { \
	if ((cr) == LLC_RSP && (pf) == 1) { \
		LLC_SETFLAG((l),P,0); \
		LLC_STOPTIMER((l),P); \
	} \
}

#define LLC_UPDATE_NR_RECEIVED(l,nr) { \
	while ((l)->llcl_nr_received != (nr)) { \
		struct mbuf *_m; \
		short seq; \
		seq = llc_seq2slot((l),(l)->llcl_nr_received); \
		if ((_m = (l)->llcl_output_buffers[seq]) != NULL) \
			m_freem(_m); \
		(l)->llcl_output_buffers[seq] = NULL; \
		LLC_INC((l)->llcl_nr_received); \
		(l)->llcl_slotsfree++; \
	} \
	(l)->llcl_retry = 0; \
	if ((l)->llcl_slotsfree < (l)->llcl_window) { \
		LLC_START_ACK_TIMER(l); \
	} else { \
		LLC_STOP_ACK_TIMER(l); \
	} \
	LLC_STARTTIMER((l),DACTION); \
}

#define LLC_SET_REMOTE_BUSY(l,a) { \
	if (LLC_GETFLAG((l),REMOTE_BUSY) == 0) { \
		LLC_SETFLAG((l),REMOTE_BUSY,1); \
		LLC_STARTTIMER((l),BUSY); \
		(a) = LLC_REMOTE_BUSY; \
	} else { \
		(a) = 0; \
	} \
}

#define LLC_CLEAR_REMOTE_BUSY(l,a) { \
	if (LLC_GETFLAG((l),REMOTE_BUSY) == 1) { \
		LLC_SETFLAG((l),REMOTE_BUSY,1); \
		LLC_STOPTIMER((l),BUSY); \
		if (LLC_STATEEQ((l),NORMAL) || \
		    LLC_STATEEQ((l),REJECT) || \
		    LLC_STATEEQ((l),BUSY)) \
			    llc_resend((l),LLC_CMD,0); \
		(a) = LLC_REMOTE_NOT_BUSY; \
	} else { \
		(a) = 0; \
	} \
}

#define LLC_DACKCMD	 0x1
#define LLC_DACKCMDPOLL	 0x2
#define LLC_DACKRSP	 0x3
#define LLC_DACKRSPFINAL 0x4

#define LLC_SENDACKNOWLEDGE(l,cmd,pf) { \
	if ((cmd) == LLC_CMD) { \
		LLC_SETFLAG((l),DACTION,((pf) == 0 ? LLC_DACKCMD : \
						    LLC_DACKCMDPOLL)); \
	} else { \
		LLC_SETFLAG((l),DACTION,((pf) == 0 ? LLC_DACKRSP : \
						    LLC_DACKRSPFINAL)); \
	} \
}

#define LLC_FRMR_W     (1<<0)
#define LLC_FRMR_X     (1<<1)
#define LLC_FRMR_Y     (1<<2)
#define LLC_FRMR_Z     (1<<3)
#define LLC_FRMR_V     (1<<4)

#define LLC_SETFRMR(l,f,cr,c) { \
	if ((f)->llc_control & 0x3) { \
		(l)->llcl_frmr_pdu0 = (f)->llc_control; \
		(l)->llcl_frmr_pdu1 = 0; \
	} else { \
		(l)->llcl_frmr_pdu0 = (f)->llc_control; \
		(l)->llcl_frmr_pdu1 = (f)->llc_control_ext; \
	} \
	LLCCSBITS((l)->llcl_frmr_control,f_vs,(l)->llcl_vs); \
	LLCCSBITS((l)->llcl_frmr_control_ext,f_cr,(cr)); \
	LLCSBITS((l)->llcl_frmr_control_ext,f_vr,(l)->llcl_vr); \
	LLCCSBITS((l)->llcl_frmr_cause,f_wxyzv,(c)); \
}

/*
 * LLC tracing levels:
 *     LLCTR_INTERESTING	interesting event,we might care to know about
 *				it,but then again,we might not ...
 *     LLCTR_SHOULDKNOW		we probably should know about this event
 *     LLCTR_URGENT		something has gone utterly wrong ...
 */
#define LLCTR_INTERESTING	1
#define LLCTR_SHOULDKNOW	2
#define LLCTR_URGENT		3

#ifdef LLCDEBUG
#define LLC_TRACE(lp,l,msg) llc_trace((lp),(l),(msg))
#else /* LLCDEBUG */
#define LLC_TRACE(lp,l,msg) /* NOOP */
#endif /* LLCDEBUG */

#define LLC_N2_VALUE	  15		  /* up to 15 retries */
#define LLC_ACK_TIMER	  10		  /*  5 secs */
#define LLC_P_TIMER	   4		  /*  2 secs */
#define LLC_BUSY_TIMER	  12		  /*  6 secs */
#define LLC_REJ_TIMER	  12		  /*  6 secs */
#define LLC_AGE_TIMER	  40		  /* 20 secs */
#define LLC_DACTION_TIMER  2		  /*  1 secs */

#if defined (_KERNEL)
extern int llc_n2;
extern int llc_ACK_timer;
extern int llc_P_timer;
extern int llc_REJ_timer;
extern int llc_BUSY_timer;
extern int llc_AGE_timer;
extern int llc_DACTION_timer;

extern int af_link_rts_init_done;

#define USES_AF_LINK_RTS { \
	if (!af_link_rts_init_done) { \
	       rn_inithead((void **)&rt_tables[AF_LINK],32); \
	       af_link_rts_init_done++; \
	       } \
	 }

extern struct ifqueue llcintrq;

extern struct llccb_q llccb_q;
extern const char *frame_names[];

/*
 * Function prototypes
 */
/* llc_input.c */
void llcintr __P((void));
int llc_input __P((struct mbuf *, ...));
void *llc_ctlinput __P((int, struct sockaddr *, void *));

/* llc_output.c */
int llc_output __P((struct mbuf *, ...));
void llc_start __P((struct llc_linkcb *));
void llc_send __P((struct llc_linkcb *, int, int, int));
void llc_resend __P((struct llc_linkcb *, int, int));
void llc_rawsend __P((struct llc_linkcb *, struct mbuf *, struct llc *, int,
		      int, int, int));

/* llc_subr.c */
int sdl_cmp __P((struct sockaddr_dl *, struct sockaddr_dl *));
void sdl_copy __P((struct sockaddr_dl *, struct sockaddr_dl *));
void sdl_swapaddr __P((struct sockaddr_dl *, struct sockaddr_dl *));
struct sockaddr_dl *sdl_getaddrif __P((struct ifnet *));
int sdl_checkaddrif __P((struct ifnet *, struct sockaddr_dl *));
int sdl_setaddrif __P((struct ifnet *, u_char *, u_char, u_char,
		       struct sockaddr_dl *));
int sdl_sethdrif __P((struct ifnet *, u_char *, u_char, u_char *, u_char,
		      u_char, struct sdl_hdr *));
struct npaidbentry *llc_setsapinfo __P((struct ifnet *, u_char, u_char,
					struct dllconfig *));
struct npaidbentry *llc_getsapinfo __P((u_char, struct ifnet *));
short llc_seq2slot __P((struct llc_linkcb *, short));
int llc_state_ADM __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_CONN __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_RESET_WAIT __P((struct llc_linkcb *, struct llc *, int,
			      int, int));
int llc_state_RESET_CHECK __P((struct llc_linkcb *, struct llc *, int,
			       int, int));
int llc_state_SETUP __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_RESET __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_D_CONN __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_ERROR __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_NBRAcore __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_NORMAL __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_BUSY __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_REJECT __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_AWAIT __P((struct llc_linkcb *, struct llc *, int, int, int));
int llc_state_AWAIT_BUSY __P((struct llc_linkcb *, struct llc *, int, int,
			      int));
int llc_state_AWAIT_REJECT __P((struct llc_linkcb *, struct llc *, int, int,
				int));
int llc_statehandler __P((struct llc_linkcb *, struct llc *, int, int, int));
void llc_init __P((void));
void llc_resetwindow __P((struct llc_linkcb *));
struct llc_linkcb *llc_newlink __P((struct sockaddr_dl *, struct ifnet *,
				    struct rtentry *, caddr_t,
				    struct rtentry *));
void llc_dellink __P((struct llc_linkcb *));
int llc_decode __P((struct llc *, struct llc_linkcb *));
int llc_anytimersup __P((struct llc_linkcb *));
const char *llc_getstatename __P((struct llc_linkcb *));
void llc_link_dump __P((struct llc_linkcb *, const char *));
void llc_trace __P((struct llc_linkcb *, int, const char *));

/* llc_timer.c */
void llc_timer __P((void));

#endif

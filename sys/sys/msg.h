/*
 * SVID compatible msg.h file
 *
 * Author:  Daniel Boulet
 *
 * Copyright 1993 Daniel Boulet and RTMX Inc.
 *
 * This system call was implemented by Daniel Boulet under contract from RTMX.
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */

#ifndef _MSG_H_
#define _MSG_H_

#ifdef KERNEL
#include "ipc.h"
#else
#include <sys/ipc.h>
#endif

/*
 * The MSG_NOERROR identifier value, the msqid_ds struct and the msg struct
 * are as defined by the SV API Intel 386 Processor Supplement.
 */

#define MSG_NOERROR	010000		/* don't complain about too long msgs */

struct msqid_ds {
    struct ipc_perm	 msg_perm;	/* msg queue permission bits */
    struct msg		*msg_first;	/* first message in the queue */
    struct msg		*msg_last;	/* last message in the queue */
    u_long		 msg_cbytes;	/* number of bytes in use on the queue */
    u_long		 msg_qnum;	/* number of msgs in the queue */
    u_long		 msg_qbytes;	/* max # of bytes on the queue */
    pid_t		 msg_lspid;	/* pid of last msgsnd() */
    pid_t		 msg_lrpid;	/* pid of last msgrcv() */
    time_t		 msg_stime;	/* time of last msgsnd() */
    long		 msg_pad1;
    time_t		 msg_rtime;	/* time of last msgrcv() */
    long		 msg_pad2;
    time_t		 msg_ctime;	/* time of last msgctl() */
    long		 msg_pad3;
    long		 msg_pad4[4];
};

struct msg {
    struct msg		*msg_next;	/* next msg in the chain */
    long		msg_type;	/* type of this message */
    					/* >0 -> type of this message */
    					/* 0 -> free header */
    ushort		msg_ts;		/* size of this message */
    short		msg_spot;	/* location of start of msg in buffer */
};

/*
 * Structure describing a message.  The SVID doesn't suggest any
 * particular name for this structure.  There is a reference in the
 * msgop man page that reads "The structure mymsg is an example of what
 * this user defined buffer might look like, and includes the following
 * members:".  This sentence is followed by two lines equivalent
 * to the mtype and mtext field declarations below.  It isn't clear
 * if "mymsg" refers to the naem of the structure type or the name of an
 * instance of the structure...
 */

struct mymsg {
    long		mtype;		/* message type (+ve integer) */
    char		mtext[1];	/* message body */
};

/*
 * Based on the configuration parameters described in an SVR2 (yes, two)
 * config(1m) man page.
 *
 * Each message is broken up and stored in segments that are msgssz bytes
 * long.  For efficiency reasons, this should be a power of two.  Also,
 * it doesn't make sense if it is less than 8 or greater than about 256.
 * Consequently, msginit in kern/sysv_msg.c checks that msgssz is a power of
 * two between 8 and 1024 inclusive (and panic's if it isn't).
 */

struct msginfo {
    int		msgmax,		/* max chars in a message */
		msgmni,		/* max message queue identifiers */
		msgmnb,		/* max chars in a queue */
		msgtql,		/* max messages in system */
		msgssz,		/* size of a message segment (see notes above) */
		msgseg;		/* number of message segments */
};
struct msginfo	msginfo;

#ifdef KERNEL

#ifndef MSGSSZ
#define MSGSSZ	8		/* Each segment must be 2^N long */
#endif

#ifndef MSGSEG
#define MSGSEG	2048		/* must be less than 32767 */
#endif

#undef MSGMAX			/* ALWAYS compute MGSMAX! */
#define MSGMAX	(MSGSSZ*MSGSEG)

#ifndef MSGMNB
#define MSGMNB	2048		/* max # of bytes in a queue */
#endif

#ifndef MSGMNI
#define MSGMNI	40
#endif

#ifndef MSGTQL
#define MSGTQL	40
#endif

/*
 * macros to convert between msqid_ds's and msqid's.
 * (specific to this implementation)
 */

#define MSQID(ix,ds)	((ix) & 0xffff | (((ds).msg_perm.seq << 16) & 0xffff0000))
#define MSQID_IX(id)	((id) & 0xffff)
#define MSQID_SEQ(id)	(((id) >> 16) & 0xffff)
#endif

/*
 * The rest of this file is specific to this particular implementation.
 */

#ifdef KERNEL

/*
 * Stuff allocated in machdep.h
 */

struct msgmap {
    short next;			/* next segment in buffer */
    				/* -1 -> available */
    				/* 0..(MSGSEG-1) -> index of next segment */
};

char *msgpool;			/* MSGMAX byte long msg buffer pool */
struct msgmap *msgmaps;		/* MSGSEG msgmap structures */
struct msg *msghdrs;		/* MSGTQL msg headers */
struct msqid_ds *msqids;	/* MSGMNI msqid_ds struct's */

#define MSG_LOCKED	01000	/* Is this msqid_ds locked? */

#endif

#ifndef KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS

int msgsys __P((int, ...));

int msgctl __P((int, int, struct msqid_ds *));
int msgget __P((key_t, int));
int msgsnd __P((int, void *, size_t, int));
int msgrcv __P((int, void*, size_t, long, int));

__END_DECLS

#endif
#endif /* !_MSG_H_ */

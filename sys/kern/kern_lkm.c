/*
 * kern_lkm.c
 *
 * functions and pseudo-device for loadable kernel modules
 *
 * 05 Jun 93	Terry Lambert		Release cleanup
 * 10 Feb 93	Terry Lambert		Original
 *
 * Copyright (c) 1992 Terrence R. Lambert.
 * All rights reserved.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: kern_lkm.c,v 1.6.2.1 1993/10/16 02:39:19 mycroft Exp $
 */

#include "param.h"
#include "systm.h"
#include "ioctl.h"
#include "tty.h"
#include "conf.h"
#include "file.h"
#include "proc.h"
#include "uio.h"
#include "kernel.h"
#include "vnode.h"
#include "malloc.h"
#include "vm/vm.h"
#include "vm/vm_param.h"
#include "vm/vm_kern.h"
#include "mount.h"
#include "exec.h"
#include "lkm.h"

#define PAGESIZE 1024		/* kmem_alloc() allocation quantum*/

#define	LKM_ALLOC	0x01
#define	LKM_WANT	0x02


#define	LKMS_IDLE	0x00
#define	LKMS_RESERVED	0x01
#define	LKMS_LOADING	0x02
#define	LKMS_LOADED	0x04
#define	LKMS_UNLOADING	0x08

static int	lkm_v = 0;
static int	lkm_state = LKMS_IDLE;

#ifndef MAXLKMS
#define	MAXLKMS		20
#endif

static struct lkm_table	lkmods[ MAXLKMS];	/* table of loaded modules*/
static struct lkm_table	*curp;			/* global for in-progress ops*/

/*ARGSUSED*/
lkmopen( dev, flag, devtype, p)
dev_t		dev;
int		flag;
int		devtype;
struct proc	*p;
{
	int	error;

	if( minor( dev) != 0)
		return( ENXIO);		/* bad minor #*/

	/*
	 * Use of the loadable kernel module device must be exclusive; we
	 * may try to remove this restriction later, but it's really no
	 * hardship.
	 */
	while( lkm_v & LKM_ALLOC) {
		if( flag & FNONBLOCK)		/* don't hang*/
			return( EBUSY);
		lkm_v |= LKM_WANT;
		/*
		 * Sleep pending unlock; we use tsleep() to allow
		 * an alarm out of the open.
		 */
		if( error = tsleep( (caddr_t)&lkm_v, TTIPRI|PCATCH, "LKM", 0))
			return( error);	/* leave LKM_WANT set -- no problem*/
	}
	lkm_v |= LKM_ALLOC;

	return( 0);		/* pseudo-device open*/
}


/*
 * l k m u n r e s e r v e
 *
 * Unreserve the memory associated with the current loaded module; done on
 * a coerced close of the lkm device (close on premature exit of modload)
 * or explicitly by modload as a result of a link failure.
 */
static int
lkmunreserve()
{
	if( lkm_state == LKMS_IDLE)
		return;

	/*
	 * Actually unreserve the memory
	 */
	kmem_free( kmem_map, curp->area, curp->size);/**/

	lkm_state = LKMS_IDLE;
}


lkmclose( dev, flag, mode, p)
dev_t		dev;
int		flag;
int		mode;
struct proc	*p;
{
	if( !( lkm_v & LKM_ALLOC)) {
#ifdef DEBUG
		printf( "LKM: close before open!\n");
#endif	/* DEBUG*/
		return( EBADF);
	}

	/* do this before waking the herd...*/
	if( !curp->used) {
		/*
		 * If we close before setting used, we have aborted
		 * by way of error or by way of close-on-exit from
		 * a premature exit of "modload".
		 */
		lkmunreserve();	/* coerce state to LKM_IDLE*/
	}

	lkm_v &= ~LKM_ALLOC;
	wakeup( (caddr_t)&lkm_v);	/* thundering herd "problem" here*/

	return( 0);		/* pseudo-device closed*/
}


/*ARGSUSED*/
lkmioctl( dev, cmd, data, flag)
dev_t		dev;
int		cmd;
caddr_t		data;
int		flag;
{
	int			err = 0;
	int			i;
	struct lmc_resrv	*resrvp;
	struct lmc_loadbuf	*loadbufp;
	struct lmc_unload	*unloadp;
	struct lmc_stat		*statp;
	int			(*funcp)();
	char			istr[ MAXLKMNAME];

	switch( cmd) {
	case LMRESERV:		/* reserve pages for a module*/
		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		resrvp = (struct lmc_resrv *)data;

		/*
		 * Find a free slot.
		 */
		for( i = 0; i < MAXLKMS; i++) {
			if( !lkmods[ i].used)
				break;
		}
		if( i == MAXLKMS) {
			err = ENOMEM;		/* no slots available*/
			break;
		}
		curp = &lkmods[ i];
		curp->id = i;		/* self reference slot offset*/

		resrvp->slot = i;		/* return slot*/

		/*
		 * Get memory for module
		 */
		curp->size = resrvp->size;

		curp->area = (char *)kmem_alloc( kmem_map, curp->size);/**/

		curp->offset = 0;		/* load offset*/

		resrvp->addr = (unsigned long)curp->area; /* ret kernel addr*/

#ifdef DEBUG
		printf( "LKM: LMRESERV (actual   = 0x%08x)\n", curp->area);
		printf( "LKM: LMRESERV (adjusted = 0x%08x)\n",
			trunc_page(curp->area));
#endif	/* DEBUG*/
		lkm_state = LKMS_RESERVED;
		break;

	case LMLOADBUF:		/* Copy in; stateful, follows LMRESERV*/
		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		loadbufp = (struct lmc_loadbuf *)data;
		if( lkm_state != LKMS_RESERVED && lkm_state != LKMS_LOADING) {
			err = ENOMEM;
			break;
		}

		/* account for odd size (non-page multiple) copyin*/
		i = MIN( curp->size - curp->offset, MODIOBUF);

		/* copy in buffer full of data*/
		if( err = copyin( (caddr_t)loadbufp->data, (caddr_t)curp->area + curp->offset, i))
			break;

		if( ( curp->offset + i) < curp->size) {
			lkm_state = LKMS_LOADING;
#ifdef DEBUG
			printf( "LKM: LMLOADBUF (loading @ %d of %d, i = %d)\n",
			curp->offset, curp->size, i);
#endif	/* DEBUG*/
		} else {
			lkm_state = LKMS_LOADED;
#ifdef DEBUG
			printf( "LKM: LMLOADBUF (loaded)\n");
#endif	/* DEBUG*/
		}
		curp->offset += MODIOBUF;
		break;

	case LMUNRESRV:		/* discard reserved pages for a module*/
		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		lkmunreserve();	/* coerce state to LKM_IDLE*/
#ifdef DEBUG
		printf( "LKM: LMUNRESERV\n");
#endif	/* DEBUG*/
		break;

	case LMREADY:		/* module loaded: call entry*/
		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		if( lkm_state != LKMS_LOADED) {

#ifdef DEBUG
			printf( "lkm_state is %02x\n", lkm_state);
#endif	/* DEBUG*/
			err = ENXIO;
			break;
		}

		curp->entry = (int (*)()) (*((int *) ( data)));

		/* call entry(load)... (assigns "private" portion)*/
		if( err = (*(curp->entry))( curp, LKM_E_LOAD, LKM_VERSION)) {
			/*
			 * Module may refuse loading or may have a
			 * version mismatch...
			 */
			lkm_state = LKMS_UNLOADING;	/* for lkmunreserve*/
			lkmunreserve();			/* free memory*/
			curp->used = 0;			/* free slot*/
			break;
		}

		curp->used = 1;
#ifdef DEBUG
		printf( "LKM: LMREADY\n");
#endif	/* DEBUG*/
		lkm_state = LKMS_IDLE;
		break;

	case LMUNLOAD:		/* unload a module*/
		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		unloadp = (struct lmc_unload *)data;

		if( ( i = unloadp->id) == -1) {		/* unload by name*/
			/*
			 * Copy name and lookup id from all loaded
			 * modules.
			 */
		 	copystr( unloadp->name, istr, MAXLKMNAME-1, NULL);
			/*
			 * look up id...
			 */
			for( i = 0; i < MAXLKMS; i++) {
				if( !lkmods[ i].used)
					continue;
				if( !strcmp( istr,
				        lkmods[ i].private.lkm_any->lkm_name))
					break;
			}
		}

		/*
		 * Range check the value; on failure, return EINVAL
		 */
		if( i < 0 || i >= MAXLKMS) {
			err = EINVAL;
			break;
		}

		curp = &lkmods[ i];

		/* call entry(unload)*/
		if( (*(curp->entry))( curp, LKM_E_UNLOAD, LKM_VERSION)) {
			err = EBUSY;
			break;
		}

		lkm_state = LKMS_UNLOADING;	/* non-idle for lkmunreserve*/
		lkmunreserve();			/* free memory*/
		curp->used = 0;			/* free slot*/
		break;

	case LMSTAT:		/* stat a module by id/name*/
		/* allow readers and writers to stat */

		statp = (struct lmc_stat *)data;

		if( ( i = statp->id) == -1) {		/* stat by name*/
			/*
			 * Copy name and lookup id from all loaded
			 * modules.
			 */
		 	copystr( statp->name, istr, MAXLKMNAME-1, NULL);
			/*
			 * look up id...
			 */
			for( i = 0; i < MAXLKMS; i++) {
				if( !lkmods[ i].used)
					continue;
				if( !strcmp( istr,
				        lkmods[ i].private.lkm_any->lkm_name))
					break;
			}

			if( i == MAXLKMS) {		/* Not found*/
				err = ENOENT;
				break;
			}
		}

		/*
		 * Range check the value; on failure, return EINVAL
		 */
		if( i < 0 || i >= MAXLKMS) {
			err = EINVAL;
			break;
		}

		curp = &lkmods[ i];

		if( !curp->used) {			/* Not found*/
			err = ENOENT;
			break;
		}

		/*
		 * Copy out stat information for this module...
		 */
		statp->id	= curp->id;
		statp->offset	= curp->private.lkm_any->lkm_offset;
		statp->type	= curp->private.lkm_any->lkm_type;
		statp->area	= curp->area;
		statp->size	= curp->size / PAGESIZE;
		statp->private	= (unsigned long)curp->private.lkm_any;
		statp->ver	= curp->private.lkm_any->lkm_ver;
		copystr( curp->private.lkm_any->lkm_name, 
			  statp->name,
			  MAXLKMNAME - 2,
			  NULL);

		break;

	default:		/* bad ioctl()...*/
		err = ENOTTY;
		break;
	}

	return (err);
}


/*********************************************************************/


/*
 * l k m n o s y s
 *
 * Acts like "nosys" but can be identified in sysent for dynamic call
 * number assignment for a limited number of calls.
 *
 * Place holder for system call slots reserved for loadable modules.
 */
lkmnosys()
{
	return( nosys());
}

/*
 * l k m e n o d e v
 *
 * Acts like "enodev", but can be identified in cdevsw and bdevsw for
 * dynamic driver major number assignment for a limited number of
 * drivers.
 *
 * Place holder for device switch slots reserved for loadable modules.
 */
int
lkmenodev()
{
	return( enodev());
}

/*********************************************************************/


int
lkmexists( lkmtp)
struct lkm_table	*lkmtp;
{
	int	i;

	/*
	 * see if name exists...
	 */
	for( i = 0; i < MAXLKMS; i++) {
		/*
		 * An unused module and the one we are testing are not
		 * considered.
		 */
		if( !lkmods[ i].used || &lkmods[ i] == lkmtp)
			continue;
		if( !strcmp( lkmtp->private.lkm_any->lkm_name,
			lkmods[ i].private.lkm_any->lkm_name))
			return( 1);		/* already loaded...*/
	}

	return( 0);		/* module not loaded...*/
}


/*
 * For the loadable system call described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_syscall( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	struct lkm_syscall	*args = lkmtp->private.lkm_syscall;
	int			i;
	int			err = 0;
	extern int		nsysent;	/* init_sysent.c*/

	switch( cmd) {
	case LKM_E_LOAD:
		/* don't load twice!*/
		if( lkmexists( lkmtp))
			return( EEXIST);
		if( ( i = args->lkm_offset) == -1) {	/* auto*/
			/*
			 * Search the table looking for a slot...
			 */
			for( i = 0; i < nsysent; i++)
				if( sysent[ i].sy_call == lkmnosys)
					break;		/* found it!*/
			/* out of allocable slots?*/
			if( i == nsysent) {
				err = ENFILE;
				break;
			}
		} else {				/* assign*/
			if( i < 0 || i >= nsysent) {
				err = EINVAL;
				break;
			}
		}

		/* save old*/
		bcopy( &sysent[ i], &(args->lkm_oldent), sizeof( struct sysent));

		/* replace with new*/
		bcopy( args->lkm_sysent, &sysent[ i], sizeof( struct sysent));

		/* done!*/
		args->lkm_offset = i;	/* slot in sysent[]*/

		break;

	case LKM_E_UNLOAD:
		/* current slot...*/
		i = args->lkm_offset;

		/* replace current slot contents with old contents*/
		bcopy( &(args->lkm_oldent), &sysent[ i], sizeof( struct sysent));

		break;

	case LKM_E_STAT:	/* no special handling...*/
		break;
	}

	return( err);
}


/*
 * For the loadable virtual file system described by the structure pointed
 * to by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_vfs( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	struct lkm_vfs		*args = lkmtp->private.lkm_vfs;
	int			i;
	int			err = 0;

	switch( cmd) {
	case LKM_E_LOAD:
		/* don't load twice!*/
		if( lkmexists( lkmtp))
			return( EEXIST);
		/*
		 * Currently, the VFS and mount code in 386BSD is malformed;
		 * this means that the per volume file system identifier is
		 * the index into the table rather than the name; this means
		 * that only the file systems already known to 386BSD are
		 * allowable, since all others don't have fixed offsets.
		 * Interestingly, Dell UNIX has this same bug with their VFS
		 * implementation, but generic AT&T SVR4 does not.
		 *
		 * I will correct the VFS code when I get a chance.
		 */
		i = args->lkm_offset;
		if( i < 0 || i > MOUNT_MAXTYPE) {
			err = EINVAL;
			break;
		}

		if( vfssw[ i] != (struct vfsops *)0) {
			err = EEXIST;
			break;
		}

		/*
		 * Set up file system
		 */
		vfssw[ i] = args->lkm_vfsops;

		/*
		 * Call init function for this VFS...
		 */
	 	( *(vfssw[ i]->vfs_init))( args->lkm_flags);

		/* done!*/
		args->lkm_offset = i;	/* slot in sysent[]*/

		break;

	case LKM_E_UNLOAD:
		/* current slot...*/
		i = args->lkm_offset;

		/* replace current slot contents with old contents*/
		vfssw[ i] = (struct vfsops *)0;

		break;

	case LKM_E_STAT:	/* no special handling...*/
		break;
	}

	return( err);
}


/*
 * For the loadable device driver described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_dev( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	struct lkm_dev		*args = lkmtp->private.lkm_dev;
	int			i;
	int			err = 0;
	extern int		nblkdev;	/* in conf.c */
	extern int		nchrdev;	/* in conf.c */

	switch( cmd) {
	case LKM_E_LOAD:
		/* don't load twice!*/
		if( lkmexists( lkmtp))
			return( EEXIST);
		switch( args->lkm_devtype) {
		case LM_DT_BLOCK:
			if( ( i = args->lkm_offset) == -1) {	/* auto*/
				/*
				 * Search the table looking for a slot...
				 */
				for( i = 0; i < nblkdev; i++)
					if( bdevsw[ i].d_open == lkmenodev)
						break;		/* found it!*/
				/* out of allocable slots?*/
				if( i == nblkdev) {
					err = ENFILE;
					break;
				}
			} else {				/* assign*/
				if( i < 0 || i >= nblkdev) {
					err = EINVAL;
					break;
				}
			}

			/* save old*/
			bcopy( &bdevsw[ i], &(args->lkm_olddev.bdev), sizeof( struct bdevsw));

			/* replace with new*/
			bcopy( args->lkm_dev.bdev, &bdevsw[ i], sizeof( struct bdevsw));

			/* done!*/
			args->lkm_offset = i;	/* slot in bdevsw[]*/
			break;

		case LM_DT_CHAR:
			if( ( i = args->lkm_offset) == -1) {	/* auto*/
				/*
				 * Search the table looking for a slot...
				 */
				for( i = 0; i < nchrdev; i++)
					if( cdevsw[ i].d_open == lkmenodev)
						break;		/* found it!*/
				/* out of allocable slots?*/
				if( i == nchrdev) {
					err = ENFILE;
					break;
				}
			} else {				/* assign*/
				if( i < 0 || i >= nchrdev) {
					err = EINVAL;
					break;
				}
			}

			/* save old*/
			bcopy( &cdevsw[ i], &(args->lkm_olddev.cdev), sizeof( struct cdevsw));

			/* replace with new*/
			bcopy( args->lkm_dev.cdev, &cdevsw[ i], sizeof( struct cdevsw));

			/* done!*/
			args->lkm_offset = i;	/* slot in cdevsw[]*/

			break;

		default:
			err = ENODEV;
			break;
		}
		break;

	case LKM_E_UNLOAD:
		/* current slot...*/
		i = args->lkm_offset;

		switch( args->lkm_devtype) {
		case LM_DT_BLOCK:
			/* replace current slot contents with old contents*/
			bcopy( &(args->lkm_olddev.bdev), &bdevsw[ i], sizeof( struct bdevsw));
			break;

		case LM_DT_CHAR:
			/* replace current slot contents with old contents*/
			bcopy( &(args->lkm_olddev.cdev), &cdevsw[ i], sizeof( struct cdevsw));
			break;

		default:
			err = ENODEV;
			break;
		}
		break;

	case LKM_E_STAT:	/* no special handling...*/
		break;
	}

	return( err);
}


#ifdef STREAMS
/*
 * For the loadable streams module described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_strmod( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	struct lkm_strmod	*args = lkmtp->private.lkm_strmod;
	int			i;
	int			err = 0;

	switch( cmd) {
	case LKM_E_LOAD:
		/* don't load twice!*/
		if( lkmexists( lkmtp))
			return( EEXIST);
		break;

	case LKM_E_UNLOAD:
		break;

	case LKM_E_STAT:	/* no special handling...*/
		break;
	}

	return( err);
}
#endif	/* STREAMS*/

/*
 * For the loadable execution class described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_exec( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	struct lkm_exec		*args = lkmtp->private.lkm_exec;
	int			i;
	int			err = 0;

	switch( cmd) {
	case LKM_E_LOAD:
		/* don't load twice!*/
		if( lkmexists( lkmtp))
			return( EEXIST);
		if( ( i = args->lkm_offset) == -1) {	/* auto*/
			/*
			 * Search the table looking for a slot...
			 */
			for( i = 0; i < nexecs; i++)
				if( execsw[ i].check == NULL)
					break;		/* found it!*/
			/* out of allocable slots?*/
			if( i == nexecs) {
				err = ENFILE;
				break;
			}
		} else {				/* assign*/
			if( i < 0 || i >= nexecs) {
				err = EINVAL;
				break;
			}
		}

		/* save old*/
		bcopy( &execsw[ i], &(args->lkm_oldexec), sizeof( struct execsw));

		/* replace with new*/
		bcopy( args->lkm_exec, &execsw[ i], sizeof( struct execsw));

		/* done!*/
		args->lkm_offset = i;	/* slot in execsw[]*/

		break;

	case LKM_E_UNLOAD:
		/* current slot...*/
		i = args->lkm_offset;

		/* replace current slot contents with old contents*/
		bcopy( &execsw[ i], &(args->lkm_oldexec), sizeof( struct execsw));

		break;

	case LKM_E_STAT:	/* no special handling...*/
		break;
	}

	return( err);
}

/*
 * This code handles the per-module type "wiring-in" of loadable modules
 * into existing kernel tables.  For "LM_MISC" modules, wiring and unwiring
 * is assumed to be done in their entry routines internal to the module
 * itself.
 */
lkmdispatch( lkmtp, cmd)
struct lkm_table	*lkmtp;	
int			cmd;
{
	int	err = 0;		/* default = success*/

	switch( lkmtp->private.lkm_any->lkm_type) {
	case LM_SYSCALL:
		err = _lkm_syscall( lkmtp, cmd);
		break;

	case LM_VFS:
		err = _lkm_vfs( lkmtp, cmd);
		break;

	case LM_DEV:
		err = _lkm_dev( lkmtp, cmd);
		break;

#ifdef STREAMS
	case LM_STRMOD:
	    {
		struct lkm_strmod	*args = lkmtp->private.lkm_strmod;
	    }
	    break;

#endif	/* STREAMS*/

	case LM_EXEC:
		err = _lkm_exec( lkmtp, cmd);
		break;

	case LM_MISC:	/* ignore content -- no "misc-specific" procedure*/
	    break;

	default:
	    err = ENXIO;	/* unknown type*/
	    break;
	}

	return( err);
}

/*	$NetBSD: sysv_shm.c,v 1.23.2.1 1994/08/31 22:12:04 cgd Exp $	*/

/*
 * Copyright (c) 1994 Adam Glass and Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Adam Glass and Charles
 *	Hannum.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/shm.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/systm.h>
#include <sys/stat.h>

#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>

/*
 * Provides the following externally accessible functions:
 *
 * shminit(void);		           initialization
 * shmexit(struct proc *)                  cleanup
 * shmfork(struct proc *, struct proc *, int) fork handling
 * shmsys(arg1, arg2, arg3, arg4);         shm{at,ctl,dt,get}(arg2, arg3, arg4)
 *
 * Structures:
 * shmsegs (an array of 'struct shmid_ds')
 * per proc array of 'struct shmmap_state'
 */

int	shmat(), shmctl(), shmdt(), shmget();
int	(*shmcalls[])() = { shmat, shmctl, shmdt, shmget };
 
#define	SHMSEG_FREE     	0x0200
#define	SHMSEG_REMOVED  	0x0400
#define	SHMSEG_ALLOCATED	0x0800
#define	SHMSEG_WANTED		0x1000

vm_map_t sysvshm_map;
int shm_last_free, shm_nused, shm_committed;

struct shm_handle {
	vm_offset_t kva;
};

struct shmmap_state {
	vm_offset_t va;
	int shmid;
};

static void shm_deallocate_segment __P((struct shmid_ds *));
static int shm_find_segment_by_key __P((key_t));
static struct shmid_ds *shm_find_segment_by_shmid __P((int));
static int shm_delete_mapping __P((struct proc *, struct shmmap_state *));

static int
shm_find_segment_by_key(key)
	key_t key;
{
	int i;

	for (i = 0; i < shminfo.shmmni; i++)
		if ((shmsegs[i].shm_perm.mode & SHMSEG_ALLOCATED) &&
		    shmsegs[i].shm_perm.key == key)
			return i;
	return -1;
}

static struct shmid_ds *
shm_find_segment_by_shmid(shmid)
	int shmid;
{
	int segnum;
	struct shmid_ds *shmseg;

	segnum = IPCID_TO_IX(shmid);
	if (segnum < 0 || segnum >= shminfo.shmmni)
		return NULL;
	shmseg = &shmsegs[segnum];
	if ((shmseg->shm_perm.mode & (SHMSEG_ALLOCATED | SHMSEG_REMOVED))
	    != SHMSEG_ALLOCATED ||
	    shmseg->shm_perm.seq != IPCID_TO_SEQ(shmid))
		return NULL;
	return shmseg;
}

static void
shm_deallocate_segment(shmseg)
	struct shmid_ds *shmseg;
{
	struct shm_handle *shm_handle;
	size_t size;

	shm_handle = shmseg->shm_internal;
	size = (shmseg->shm_segsz + CLOFSET) & ~CLOFSET;
	vm_deallocate(sysvshm_map, shm_handle->kva, size);
	free((caddr_t)shm_handle, M_SHM);
	shmseg->shm_internal = NULL;
	shm_committed -= btoc(size);
	shmseg->shm_perm.mode = SHMSEG_FREE;
	shm_nused--;
}

static int
shm_delete_mapping(p, shmmap_s)
	struct proc *p;
	struct shmmap_state *shmmap_s;
{
	struct shmid_ds *shmseg;
	int segnum, result;
	size_t size;
	
	segnum = IPCID_TO_IX(shmmap_s->shmid);
	shmseg = &shmsegs[segnum];
	size = (shmseg->shm_segsz + CLOFSET) & ~CLOFSET;
	result = vm_deallocate(&p->p_vmspace->vm_map, shmmap_s->va, size);
	if (result != KERN_SUCCESS)
		return EINVAL;
	shmmap_s->shmid = -1;
	shmseg->shm_dtime = time.tv_sec;
	if ((--shmseg->shm_nattch <= 0) &&
	    (shmseg->shm_perm.mode & SHMSEG_REMOVED)) {
		shm_deallocate_segment(shmseg);
		shm_last_free = segnum;
	}
	return 0;
}

struct shmdt_args {
	void *shmaddr;
};
int
shmdt(p, uap, retval)
	struct proc *p;
	struct shmdt_args *uap;
	int *retval;
{
	struct shmmap_state *shmmap_s;
	int i;

	shmmap_s = (struct shmmap_state *)p->p_vmspace->vm_shm;
	for (i = 0; i < shminfo.shmseg; i++, shmmap_s++)
		if (shmmap_s->shmid != -1 &&
		    shmmap_s->va == (vm_offset_t)uap->shmaddr)
			break;
	if (i == shminfo.shmseg)
		return EINVAL;
	return shm_delete_mapping(p, shmmap_s);
}

struct shmat_args {
	int shmid;
	void *shmaddr;
	int shmflg;
};
int
shmat(p, uap, retval)
	struct proc *p;
	struct shmat_args *uap;
	int *retval;
{
	int error, i, flags;
	struct ucred *cred = p->p_ucred;
	struct shmid_ds *shmseg;
	struct shmmap_state *shmmap_s = NULL;
	vm_offset_t attach_va;
	vm_prot_t prot;
	vm_size_t size;

	shmmap_s = (struct shmmap_state *)p->p_vmspace->vm_shm;
	if (shmmap_s == NULL) {
		size = shminfo.shmseg * sizeof(struct shmmap_state);
		shmmap_s = malloc(size, M_SHM, M_WAITOK);
		for (i = 0; i < shminfo.shmseg; i++)
			shmmap_s[i].shmid = -1;
		p->p_vmspace->vm_shm = (caddr_t)shmmap_s;
	}
	shmseg = shm_find_segment_by_shmid(uap->shmid);
	if (shmseg == NULL)
		return EINVAL;
	if (error = ipcperm(cred, &shmseg->shm_perm,
	    (uap->shmflg & SHM_RDONLY) ? IPC_R : IPC_R|IPC_W))
		return error;
	for (i = 0; i < shminfo.shmseg; i++) {
		if (shmmap_s->shmid == -1)
			break;
		shmmap_s++;
	}
	if (i >= shminfo.shmseg)
		return EMFILE;
	size = (shmseg->shm_segsz + CLOFSET) & ~CLOFSET;
	prot = VM_PROT_READ;
	if ((uap->shmflg & SHM_RDONLY) == 0)
		prot |= VM_PROT_WRITE;
	flags = MAP_ANON | MAP_SHARED;
	if (uap->shmaddr) {
		flags |= MAP_FIXED;
		if (uap->shmflg & SHM_RND) 
			attach_va = (vm_offset_t)uap->shmaddr & ~(SHMLBA-1);
		else if (((vm_offset_t)uap->shmaddr & (SHMLBA-1)) == 0)
			attach_va = (vm_offset_t)uap->shmaddr;
		else
			return EINVAL;
	} else {
		/* This is just a hint to vm_mmap() about where to put it. */
		attach_va = round_page(p->p_vmspace->vm_daddr + MAXDSIZ);
	}
	error = vm_mmap(&p->p_vmspace->vm_map, &attach_va, size, prot,
	    VM_PROT_DEFAULT, flags, (caddr_t) uap->shmid, 0);
	if (error)
		return error;
	shmmap_s->va = attach_va;
	shmmap_s->shmid = uap->shmid;
	shmseg->shm_lpid = p->p_pid;
	shmseg->shm_atime = time.tv_sec;
	shmseg->shm_nattch++;
	*retval = attach_va;
	return 0;
}

struct shmctl_args {
	int shmid;
	int cmd;
	struct shmat_ds *ubuf;
};
int
shmctl(p, uap, retval)
	struct proc *p;
	struct shmctl_args *uap;
	int *retval;
{
	int error, segnum;
	struct ucred *cred = p->p_ucred;
	struct shmid_ds inbuf;
	struct shmid_ds *shmseg;

	shmseg = shm_find_segment_by_shmid(uap->shmid);
	if (shmseg == NULL)
		return EINVAL;
	switch (uap->cmd) {
	case IPC_STAT:
		if (error = ipcperm(cred, &shmseg->shm_perm, IPC_R))
			return error;
		if (error = copyout((caddr_t)shmseg, uap->ubuf, sizeof(inbuf)))
			return error;
		break;
	case IPC_SET:
		if (error = ipcperm(cred, &shmseg->shm_perm, IPC_M))
			return error;
		if (error = copyin(uap->ubuf, (caddr_t)&inbuf, sizeof(inbuf)))
			return error;
		shmseg->shm_perm.uid = inbuf.shm_perm.uid;
		shmseg->shm_perm.gid = inbuf.shm_perm.gid;
		shmseg->shm_perm.mode =
		    (shmseg->shm_perm.mode & ~ACCESSPERMS) |
		    (inbuf.shm_perm.mode & ACCESSPERMS);
		shmseg->shm_ctime = time.tv_sec;
		break;
	case IPC_RMID:
		if (error = ipcperm(cred, &shmseg->shm_perm, IPC_M))
			return error;
		shmseg->shm_perm.key = IPC_PRIVATE;
		shmseg->shm_perm.mode |= SHMSEG_REMOVED;
		if (shmseg->shm_nattch <= 0) {
			shm_deallocate_segment(shmseg);
			shm_last_free = IPCID_TO_IX(uap->shmid);
		}
		break;
#if 0
	case SHM_LOCK:
	case SHM_UNLOCK:
#endif
	default:
		return EINVAL;
	}
	return 0;
}

struct shmget_args {
	key_t key;
	size_t size;
	int shmflg;
};
static int
shmget_existing(p, uap, mode, segnum, retval)
	struct proc *p;
	struct shmget_args *uap;
	int mode;
	int segnum;
	int *retval;
{
	struct shmid_ds *shmseg;
	struct ucred *cred = p->p_ucred;
	int error;

	shmseg = &shmsegs[segnum];
	if (shmseg->shm_perm.mode & SHMSEG_REMOVED) {
		/*
		 * This segment is in the process of being allocated.  Wait
		 * until it's done, and look the key up again (in case the
		 * allocation failed or it was freed).
		 */
		shmseg->shm_perm.mode |= SHMSEG_WANTED;
		if (error =
		    tsleep((caddr_t)shmseg, PLOCK | PCATCH, "shmget", 0))
			return error;
		return EAGAIN;
	}
	if (error = ipcperm(cred, &shmseg->shm_perm, mode))
		return error;
	if (uap->size && uap->size > shmseg->shm_segsz)
		return EINVAL;
	if (uap->shmflg & (IPC_CREAT | IPC_EXCL) == (IPC_CREAT | IPC_EXCL))
		return EEXIST;
	*retval = IXSEQ_TO_IPCID(segnum, shmseg->shm_perm);
	return 0;
}

static int
shmget_allocate_segment(p, uap, mode, retval)
	struct proc *p;
	struct shmget_args *uap;
	int mode;
	int *retval;
{
	int i, segnum, result, shmid, size;
	struct ucred *cred = p->p_ucred;
	struct shmid_ds *shmseg;
	struct shm_handle *shm_handle;
	
	if (uap->size < shminfo.shmmin || uap->size > shminfo.shmmax)
		return EINVAL;
	if (shm_nused >= shminfo.shmmni) /* any shmids left? */
		return ENOSPC;
	size = (uap->size + CLOFSET) & ~CLOFSET;
	if (shm_committed + btoc(size) > shminfo.shmall)
		return ENOMEM;
	if (shm_last_free < 0) {
		for (i = 0; i < shminfo.shmmni; i++)
			if (shmsegs[i].shm_perm.mode & SHMSEG_FREE)
				break;
		if (i == shminfo.shmmni)
			panic("shmseg free count inconsistent");
		segnum = i;
	} else  {
		segnum = shm_last_free;
		shm_last_free = -1;
	}
	shmseg = &shmsegs[segnum];
	/*
	 * In case we sleep in malloc(), mark the segment present but deleted
	 * so that noone else tries to create the same key.
	 */
	shmseg->shm_perm.mode = SHMSEG_ALLOCATED | SHMSEG_REMOVED;
	shmseg->shm_perm.key = uap->key;
	shmseg->shm_perm.seq = (shmseg->shm_perm.seq + 1) & 0x7fff;
	shm_handle = (struct shm_handle *)
	    malloc(sizeof(struct shm_handle), M_SHM, M_WAITOK);
	shmid = IXSEQ_TO_IPCID(segnum, shmseg->shm_perm);
	result = vm_mmap(sysvshm_map, &shm_handle->kva, size, VM_PROT_ALL,
	    VM_PROT_DEFAULT, MAP_ANON, (caddr_t) shmid, 0);
	if (result != KERN_SUCCESS) {
		shmseg->shm_perm.mode = SHMSEG_FREE;
		shm_last_free = segnum;
		free((caddr_t)shm_handle, M_SHM);
		/* Just in case. */
		wakeup((caddr_t)shmseg);
		return ENOMEM;
	}
	shmseg->shm_internal = shm_handle;
	shmseg->shm_perm.cuid = shmseg->shm_perm.uid = cred->cr_uid;
	shmseg->shm_perm.cgid = shmseg->shm_perm.gid = cred->cr_gid;
	shmseg->shm_perm.mode = (shmseg->shm_perm.mode & SHMSEG_WANTED) |
	    (mode & ACCESSPERMS) | SHMSEG_ALLOCATED;
	shmseg->shm_segsz = uap->size;
	shmseg->shm_cpid = p->p_pid;
	shmseg->shm_lpid = shmseg->shm_nattch = 0;
	shmseg->shm_atime = shmseg->shm_dtime = 0;
	shmseg->shm_ctime = time.tv_sec;
	shm_committed += btoc(size);
	shm_nused++;
	if (shmseg->shm_perm.mode & SHMSEG_WANTED) {
		/*
		 * Somebody else wanted this key while we were asleep.  Wake
		 * them up now.
		 */
		shmseg->shm_perm.mode &= ~SHMSEG_WANTED;
		wakeup((caddr_t)shmseg);
	}
	*retval = shmid;
	return 0;
}

int
shmget(p, uap, retval)
	struct proc *p;
	struct shmget_args *uap;
	int *retval;
{
	int segnum, mode, error;
	struct shmid_ds *shmseg;

	mode = uap->shmflg & ACCESSPERMS;
	if (uap->key != IPC_PRIVATE) {
	again:
		segnum = shm_find_segment_by_key(uap->key);
		if (segnum >= 0) {
			error = shmget_existing(p, uap, mode, segnum, retval);
			if (error == EAGAIN)
				goto again;
			return error;
		}
		if ((uap->shmflg & IPC_CREAT) == 0) 
			return ENOENT;
	}
	return shmget_allocate_segment(p, uap, mode, retval);
}

struct shmsys_args {
	u_int	which;
};
int
shmsys(p, uap, retval)
	struct proc *p;
	struct shmsys_args *uap;
	int *retval;
{

	if (uap->which >= sizeof(shmcalls)/sizeof(shmcalls[0]))
		return EINVAL;
	return ((*shmcalls[uap->which])(p, &uap[1], retval));
}

void
shmfork(p1, p2, isvfork)
	struct proc *p1, *p2;
	int isvfork;
{
	struct shmmap_state *shmmap_s;
	size_t size;
	int i;

	size = shminfo.shmseg * sizeof(struct shmmap_state);
	shmmap_s = malloc(size, M_SHM, M_WAITOK);
	bcopy((caddr_t)p1->p_vmspace->vm_shm, (caddr_t)shmmap_s, size);
	p2->p_vmspace->vm_shm = (caddr_t)shmmap_s;
	for (i = 0; i < shminfo.shmseg; i++, shmmap_s++)
		if (shmmap_s->shmid != -1)
			shmsegs[IPCID_TO_IX(shmmap_s->shmid)].shm_nattch++;
}

void
shmexit(p)
	struct proc *p;
{
	struct shmmap_state *shmmap_s;
	struct shmid_ds *shmseg;
	int i;

	shmmap_s = (struct shmmap_state *)p->p_vmspace->vm_shm;
	for (i = 0; i < shminfo.shmseg; i++, shmmap_s++)
		if (shmmap_s->shmid != -1)
			shm_delete_mapping(p, shmmap_s);
	free((caddr_t)p->p_vmspace->vm_shm, M_SHM);
	p->p_vmspace->vm_shm = NULL;
}

void
shminit()
{
	int i;
	vm_offset_t garbage1, garbage2;

	/* actually this *should* be pageable.  SHM_{LOCK,UNLOCK} */
	sysvshm_map = kmem_suballoc(kernel_map, &garbage1, &garbage2,
				    shminfo.shmall * NBPG, TRUE);
	for (i = 0; i < shminfo.shmmni; i++) {
		shmsegs[i].shm_perm.mode = SHMSEG_FREE;
		shmsegs[i].shm_perm.seq = 0;
	}
	shm_last_free = 0;
	shm_nused = 0;
	shm_committed = 0;
}

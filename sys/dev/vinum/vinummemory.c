/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
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
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinummemory.c,v 1.1.1.1.4.3 2004/09/18 14:52:13 skrll Exp $
 * $FreeBSD$
 */

#include <dev/vinum/vinumhdr.h>

#ifdef VINUMDEBUG
#include <dev/vinum/request.h>
extern struct rqinfo rqinfo[];
extern struct rqinfo *rqip;
int rqinfo_size = RQINFO_SIZE;				    /* for debugger */

#undef longjmp						    /* this was defined as LongJmp */
/* XXX add tests for buf validity */
void
LongJmp(label_t * buf)
{
    longjmp(buf);
}

#endif /* VINUMDEBUG */

/* find the base name of a path name */
char *
basename(char *file)
{
    char *f = strrchr(file, '/');			    /* chop off dirname if present */

    if (f == NULL)
	return file;
    else
	return ++f;					    /* skip the / */
}

void
expand_table(void **table, int oldsize, int newsize)
{
    if (newsize > oldsize) {
	int *temp;
	int s;

	s = splhigh();
	temp = (int *) Malloc(newsize);			    /* allocate a new table */
	CHECKALLOC(temp, "vinum: Can't expand table\n");
	bzero((char *) temp, newsize);			    /* clean it all out */
	if (*table != NULL) {				    /* already something there, */
	    bcopy((char *) *table, (char *) temp, oldsize); /* copy it to the old table */
	    Free(*table);
	}
	*table = temp;
	splx(s);
    }
}

#ifdef VINUMDEBUG					    /* XXX debug */
#define MALLOCENTRIES 16384
int malloccount = 0;
int highwater = 0;					    /* highest index ever allocated */
struct mc malloced[MALLOCENTRIES];

#define FREECOUNT 64
int freecount = FREECOUNT;				    /* for debugger */
int lastfree = 0;
struct mc freeinfo[FREECOUNT];

int total_malloced;
static int mallocseq = 0;

caddr_t
MMalloc(int size, char *file, int line)
{
    int s;
    caddr_t result;
    int i;

    if (malloccount >= MALLOCENTRIES) {			    /* too many */
	log(LOG_ERR, "vinum: can't allocate table space to trace memory allocation");
	return 0;					    /* can't continue */
    }
    /* Wait for malloc if we can */
    /*
     * XXX We can wait if we're in process context.  How do we tell?
     */
    result = malloc(size, M_DEVBUF, M_NOWAIT);
    if (result == NULL)
	log(LOG_ERR, "vinum: can't allocate %d bytes from %s:%d\n", size, file, line);
    else {
	s = splhigh();
	for (i = 0; i < malloccount; i++) {
	    if (((result + size) > malloced[i].address)
		&& (result < malloced[i].address + malloced[i].size)) /* overlap */
		panic("Malloc overlap");
	}
	if (result) {
	    char *f = basename(file);

	    i = malloccount++;
	    total_malloced += size;
	    microtime(&malloced[i].time);
	    malloced[i].seq = mallocseq++;
	    malloced[i].size = size;
	    malloced[i].line = line;
	    malloced[i].address = result;
	    bcopy(f, malloced[i].file, min(strlen(f), MCFILENAMELEN - 1));
	    malloced[i].file[MCFILENAMELEN - 1] = '\0';
	}
	if (malloccount > highwater)
	    highwater = malloccount;
	splx(s);
    }
    return result;
}

void
FFree(void *mem, char *file, int line)
{
    int s;
    int i;

    s = splhigh();
    for (i = 0; i < malloccount; i++) {
	if ((caddr_t) mem == malloced[i].address) {	    /* found it */
	    bzero(mem, malloced[i].size);		    /* XXX */
	    free(mem, M_DEVBUF);
	    malloccount--;
	    total_malloced -= malloced[i].size;
	    if (debug & DEBUG_MEMFREE) {		    /* keep track of recent frees */
		char *f = strrchr(file, '/');		    /* chop off dirname if present */

		if (f == NULL)
		    f = file;
		else
		    f++;				    /* skip the / */

		microtime(&freeinfo[lastfree].time);
		freeinfo[lastfree].seq = malloced[i].seq;
		freeinfo[lastfree].size = malloced[i].size;
		freeinfo[lastfree].line = line;
		freeinfo[lastfree].address = mem;
		bcopy(f, freeinfo[lastfree].file, min(strlen(f), MCFILENAMELEN - 1));
		freeinfo[lastfree].file[MCFILENAMELEN - 1] = '\0';
		if (++lastfree == FREECOUNT)
		    lastfree = 0;
	    }
	    if (i < malloccount)			    /* more coming after */
		bcopy(&malloced[i + 1], &malloced[i], (malloccount - i) * sizeof(struct mc));
	    splx(s);
	    return;
	}
    }
    splx(s);
    log(LOG_ERR,
	"Freeing unallocated data at 0x%p from %s, line %d\n",
	mem,
	file,
	line);
    panic("Free");
}

void
vinum_meminfo(caddr_t data)
{
    struct meminfo *m = (struct meminfo *) data;

    m->mallocs = malloccount;
    m->total_malloced = total_malloced;
    m->malloced = malloced;
    m->highwater = highwater;
}

int
vinum_mallocinfo(caddr_t data)
{
    struct mc *m = (struct mc *) data;
    unsigned int ent = m->seq;				    /* index of entry to return */

    if (ent >= malloccount)
	return ENOENT;
    m->address = malloced[ent].address;
    m->size = malloced[ent].size;
    m->line = malloced[ent].line;
    m->seq = malloced[ent].seq;
    bcopy(malloced[ent].file, m->file, MCFILENAMELEN);
    return 0;
}

/*
 * return the nth request trace buffer entry.  This
 * is indexed back from the current entry (which
 * has index 0)
 */
int
vinum_rqinfo(caddr_t data)
{
    struct rqinfo *rq = (struct rqinfo *) data;
    int ent = *(int *) data;				    /* 1st word is index */
    int lastent = rqip - rqinfo;			    /* entry number of current entry */

    if (ent >= RQINFO_SIZE)				    /* out of the table */
	return ENOENT;
    if ((ent = lastent - ent - 1) < 0)
	ent += RQINFO_SIZE;				    /* roll over backwards */
    bcopy(&rqinfo[ent], rq, sizeof(struct rqinfo));
    return 0;
}
#endif

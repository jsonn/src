/*
 * Copyright (c) 1995, 1996, 1997, 1999 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"

RCSID("$Id: rnd_keys.c,v 1.1.1.1.4.2 2000/06/16 18:45:43 thorpej Exp $");
#endif

#include <des.h>
#include <des_locl.h>
#ifdef KRB5
#include <krb5-types.h>
#elif defined(KRB4)
#include <ktypes.h>
#endif

#include <string.h>

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif

/*
 * Generate "random" data by checksumming a file.
 *
 * Returns -1 if there were any problems with permissions or I/O
 * errors.
 */
static
int
sumFile (const char *name, int len, void *res)
{
  u_int32_t sum[2];
  u_int32_t buf[1024*2];
  int fd, i;

  fd = open (name, 0);
  if (fd < 0)
    return -1;

  while (len > 0)
    {
      int n = read(fd, buf, sizeof(buf));
      if (n < 0)
	{
	  close(fd);
	  return n;
	}
      for (i = 0; i < (n/sizeof(buf[0])); i++)
	{
	  sum[0] += buf[i];
	  i++;
	  sum[1] += buf[i];
	}
      len -= n;
    }
  close (fd);
  memcpy (res, &sum, sizeof(sum));
  return 0;
}

#if 0
static
int
md5sumFile (const char *name, int len, int32_t sum[4])
{
  int32_t buf[1024*2];
  int fd, cnt;
  struct md5 md5;

  fd = open (name, 0);
  if (fd < 0)
    return -1;

  md5_init(&md5);
  while (len > 0)
    {
      int n = read(fd, buf, sizeof(buf));
      if (n < 0)
	{
	  close(fd);
	  return n;
	}
      md5_update(&md5, buf, n);
      len -= n;
    }
  md5_finito(&md5, (unsigned char *)sum);
  close (fd);
  return 0;
}
#endif

/*
 * Create a sequence of random 64 bit blocks.
 * The sequence is indexed with a long long and 
 * based on an initial des key used as a seed.
 */
static des_key_schedule sequence_seed;
static u_int32_t sequence_index[2];

/* 
 * Random number generator based on ideas from truerand in cryptolib
 * as described on page 424 in Applied Cryptography 2 ed. by Bruce
 * Schneier.
 */

static volatile int counter;
static volatile unsigned char *gdata; /* Global data */
static volatile int igdata;	/* Index into global data */
static int gsize;

#if !defined(WIN32) && !defined(__EMX__) && !defined(__OS2__) && !defined(__CYGWIN32__)
/* Visual C++ 4.0 (Windows95/NT) */

static
RETSIGTYPE
sigALRM(int sig)
{
    if (igdata < gsize)
	gdata[igdata++] ^= counter & 0xff;

#ifndef HAVE_SIGACTION
    signal(SIGALRM, sigALRM); /* Reinstall SysV signal handler */
#endif
    SIGRETURN(0);
}

#endif

#if !defined(HAVE_RANDOM) && defined(HAVE_RAND)
#ifndef srandom
#define srandom srand
#endif
#ifndef random
#define random rand
#endif
#endif

static void
des_not_rand_data(unsigned char *data, int size)
{
  int i;

  srandom (time (NULL));

  for(i = 0; i < size; ++i)
    data[i] ^= random() % 0x100;
}

#if !defined(WIN32) && !defined(__EMX__) && !defined(__OS2__) && !defined(__CYGWIN32__)

#ifndef HAVE_SETITIMER
static void
pacemaker(struct timeval *tv)
{
    fd_set fds;
    pid_t pid;
    pid = getppid();
    while(1){
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	select(1, &fds, NULL, NULL, tv);
	kill(pid, SIGALRM);
    }
}
#endif

#ifdef HAVE_SIGACTION
/* XXX ugly hack, should perhaps use function from roken */
static RETSIGTYPE 
(*fake_signal(int sig, RETSIGTYPE (*f)(int)))(int)
{
    struct sigaction sa, osa;
    sa.sa_handler = f;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, &osa);
    return osa.sa_handler;
}
#define signal(S, F) fake_signal((S), (F))
#endif

/*
 * Generate size bytes of "random" data using timed interrupts.
 * It takes about 40ms/byte random data.
 * It's not neccessary to be root to run it.
 */
void
des_rand_data(unsigned char *data, int size)
{
    struct itimerval tv, otv;
    RETSIGTYPE (*osa)(int);
    int i, j;
#ifndef HAVE_SETITIMER 
    RETSIGTYPE (*ochld)(int);
    pid_t pid;
#endif
    char *rnd_devices[] = {"/dev/random",
			   "/dev/srandom",
			   "/dev/urandom",
			   NULL};
    char **p;

    for(p = rnd_devices; *p; p++) {
      int fd = open(*p, O_RDONLY | O_NDELAY);
      
      if(fd >= 0 && read(fd, data, size) == size) {
	close(fd);
	return;
      }
      close(fd);
    }

    /* Paranoia? Initialize data from /dev/mem if we can read it. */
    if (size >= 8)
      sumFile("/dev/mem", (1024*1024*2), data);

    gdata = data;
    gsize = size;
    igdata = 0;

    osa = signal(SIGALRM, sigALRM);
  
    /* Start timer */
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = 10 * 1000; /* 10 ms */
    tv.it_interval = tv.it_value;
#ifdef HAVE_SETITIMER
    setitimer(ITIMER_REAL, &tv, &otv);
#else
    ochld = signal(SIGCHLD, SIG_IGN);
    pid = fork();
    if(pid == -1){
	signal(SIGCHLD, ochld != SIG_ERR ? ochld : SIG_DFL);
	des_not_rand_data(data, size);
	return;
    }
    if(pid == 0)
	pacemaker(&tv.it_interval);
#endif

    for(i = 0; i < 4; i++) {
	for (igdata = 0; igdata < size;) /* igdata++ in sigALRM */
	    counter++;
	for (j = 0; j < size; j++) /* Only use 2 bits each lap */
	    gdata[j] = (gdata[j]>>2) | (gdata[j]<<6);
    }
#ifdef HAVE_SETITIMER
    setitimer(ITIMER_REAL, &otv, 0);
#else
    kill(pid, SIGKILL);
    while(waitpid(pid, NULL, 0) != pid);
    signal(SIGCHLD, ochld != SIG_ERR ? ochld : SIG_DFL);
#endif
    signal(SIGALRM, osa != SIG_ERR ? osa : SIG_DFL);
}
#else
void
des_rand_data(unsigned char *p, int s)
{
  des_not_rand_data (p, s);
}
#endif

void
des_generate_random_block(des_cblock *block)
{
  des_rand_data((unsigned char *)block, sizeof(*block));
}

/*
 * Generate a "random" DES key.
 */
void
des_rand_data_key(des_cblock *key)
{
    unsigned char data[8];
    des_key_schedule sched;
    do {
	des_rand_data(data, sizeof(data));
	des_rand_data((unsigned char*)key, sizeof(des_cblock));
	des_set_odd_parity(key);
	des_key_sched(key, sched);
	des_ecb_encrypt(&data, key, sched, DES_ENCRYPT);
	memset(&data, 0, sizeof(data));
	memset(&sched, 0, sizeof(sched));
	des_set_odd_parity(key);
    } while(des_is_weak_key(key));
}

/*
 * Generate "random" data by checksumming /dev/mem
 *
 * It's neccessary to be root to run it. Returns -1 if there were any
 * problems with permissions.
 */
int
des_mem_rand8(unsigned char *data)
{
  return 1;
}

/*
 * In case the generator does not get initialized use this as fallback.
 */
static int initialized;

static void
do_initialize(void)
{
    des_cblock default_seed;
    do {
	des_generate_random_block(&default_seed);
	des_set_odd_parity(&default_seed);
    } while (des_is_weak_key(&default_seed));
    des_init_random_number_generator(&default_seed);
}

#define zero_long_long(ll) do { ll[0] = ll[1] = 0; } while (0)

#define incr_long_long(ll) do { if (++ll[0] == 0) ++ll[1]; } while (0)

#define set_sequence_number(ll) \
memcpy((char *)sequence_index, (ll), sizeof(sequence_index));

/*
 * Set the sequnce number to this value (a long long).
 */
void
des_set_sequence_number(unsigned char *ll)
{
    set_sequence_number(ll);
}

/*
 * Set the generator seed and reset the sequence number to 0.
 */
void
des_set_random_generator_seed(des_cblock *seed)
{
    des_key_sched(seed, sequence_seed);
    zero_long_long(sequence_index);
    initialized = 1;
}

/*
 * Generate a sequence of random des keys
 * using the random block sequence, fixup
 * parity and skip weak keys.
 */
int
des_new_random_key(des_cblock *key)
{
    if (!initialized)
	do_initialize();

    do {
	des_ecb_encrypt((des_cblock *) sequence_index,
			key,
			sequence_seed,
			DES_ENCRYPT);
	incr_long_long(sequence_index);
	/* random key must have odd parity and not be weak */
	des_set_odd_parity(key);
    } while (des_is_weak_key(key));
    return(0);
}

/*
 * des_init_random_number_generator:
 *
 * Initialize the sequence of random 64 bit blocks.  The input seed
 * can be a secret key since it should be well hidden and is also not
 * kept.
 *
 */
void 
des_init_random_number_generator(des_cblock *seed)
{
    struct timeval now;
    des_cblock uniq;
    des_cblock new_key;

    gettimeofday(&now, (struct timezone *)0);
    des_generate_random_block(&uniq);

    /* Pick a unique random key from the shared sequence. */
    des_set_random_generator_seed(seed);
    set_sequence_number((unsigned char *)&uniq);
    des_new_random_key(&new_key);

    /* Select a new nonshared sequence, */
    des_set_random_generator_seed(&new_key);

    /* and use the current time to pick a key for the new sequence. */
    set_sequence_number((unsigned char *)&now);
    des_new_random_key(&new_key);
    des_set_random_generator_seed(&new_key);
}

/* This is for backwards compatibility. */
void
des_random_key(des_cblock ret)
{
    des_new_random_key((des_cblock *)ret);
}

#ifdef TESTRUN
int
main()
{
    unsigned char data[8];
    int i;

    while (1)
        {
	    if (sumFile("/dev/mem", (1024*1024*8), data) != 0)
	      { perror("sumFile"); exit(1); }
            for (i = 0; i < 8; i++)
                printf("%02x", data[i]);
            printf("\n");
        }
}
#endif

#ifdef TESTRUN2
int
main()
{
    des_cblock data;
    int i;

    while (1)
        {
	    do_initialize();
            des_random_key(data);
            for (i = 0; i < 8; i++)
                printf("%02x", data[i]);
            printf("\n");
        }
}
#endif

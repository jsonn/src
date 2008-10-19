#include <sys/types.h>
#include <sys/time.h>
#include <sys/sockio.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEST_ADDR "204.152.190.12"	/* www.NetBSD.org */
#define DEST_PORT 80			/* take a wild guess */

/*
 * If we are running with the full networking stack, configure
 * virtual interface
 */
#ifdef FULL_NETWORK_STACK
#define MYADDR "10.181.181.11"
#define MYBCAST "10.181.181.255"
#define MYMASK "255.255.255.0"
#define MYGW "10.181.181.1"
#define IFNAME "virt0" /* XXX: hardcoded */

int rump_virtif_create(void *, void *); /* XXX: bad hack, prototype is wrong */
static void
configure_interface()
{
        struct sockaddr_in *sin;
	struct sockaddr_in sinstore;
	struct ifaliasreq ia;
	size_t len;
	struct {
		struct rt_msghdr m_rtm;
		uint8_t m_space;
	} m_rtmsg;
#define rtm m_rtmsg.m_rtm
	uint8_t *bp = &m_rtmsg.m_space;
	int s, rv, error;

	if ((rv = rump_virtif_create(NULL, NULL)) != 0) {
		printf("could not configure interface %d\n", rv);
		exit(1);
	}

	/* get a socket for configuring the interface */
	s = rump_sys___socket30(PF_INET, SOCK_DGRAM, 0, &error);
	if (s == -1)
		errx(1, "configuration socket %d (%s)", error, strerror(error));

	/* fill out struct ifaliasreq */
	memset(&ia, 0, sizeof(ia));
	strcpy(ia.ifra_name, IFNAME);
	sin = (struct sockaddr_in *)&ia.ifra_addr;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_addr.s_addr = inet_addr(MYADDR);

	sin = (struct sockaddr_in *)&ia.ifra_broadaddr;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_addr.s_addr = inet_addr(MYBCAST);

	sin = (struct sockaddr_in *)&ia.ifra_mask;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_addr.s_addr = inet_addr(MYMASK);

	/* toss to the configuration socket and see what it thinks */
	rv = rump_sys_ioctl(s, SIOCAIFADDR, &ia, &error);
	if (rv)
		errx(1, "SIOCAIFADDR %d (%s)", error, strerror(error));
	rump_sys_close(s, &error);

	/* open routing socket and configure our default router */
	s = rump_sys___socket30(PF_ROUTE, SOCK_RAW, 0, &error);
	if (s == -1)
		errx(1, "routing socket %d (%s)", error, strerror(error));

	/* create routing message */
        memset(&m_rtmsg, 0, sizeof(m_rtmsg));
        rtm.rtm_type = RTM_ADD;
        rtm.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
        rtm.rtm_version = RTM_VERSION;
        rtm.rtm_seq = 2;
        rtm.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;

	/* dst */
        memset(&sinstore, 0, sizeof(sinstore));
        sinstore.sin_family = AF_INET;
        sinstore.sin_len = sizeof(sinstore);
        memcpy(bp, &sinstore, sizeof(sinstore));
        bp += sizeof(sinstore);

	/* gw */
        memset(&sinstore, 0, sizeof(sinstore));
        sinstore.sin_family = AF_INET;
        sinstore.sin_len = sizeof(sinstore);
        sinstore.sin_addr.s_addr = inet_addr(MYGW);
        memcpy(bp, &sinstore, sizeof(sinstore));
        bp += sizeof(sinstore);

	/* netmask */
        memset(&sinstore, 0, sizeof(sinstore));
        sinstore.sin_family = AF_INET;
        sinstore.sin_len = sizeof(sinstore);
        memcpy(bp, &sinstore, sizeof(sinstore));
        bp += sizeof(sinstore);

        len = bp - (uint8_t *)&m_rtmsg;
        rtm.rtm_msglen = len;

	/* stuff that to the routing socket and wait for happy days */
	if (rump_sys_write(s, &m_rtmsg, len, &error) != len)
		errx(1, "routing incomplete %d (%s)", error, strerror(error));
	rump_sys_close(s, &error);
}
#endif /* FULL_NETWORK_STACK */

int
main()
{
	char buf[65536];
	struct sockaddr_in sin;
	struct timeval tv;
	ssize_t n;
	size_t off;
	int s, error;

	if (rump_init())
		errx(1, "rump_init failed");

#ifdef FULL_NETWORK_STACK
	configure_interface();
#endif

	s = rump_sys___socket30(PF_INET, SOCK_STREAM, 0, &error);
	if (s == -1)
		errx(1, "can't open socket: %d (%s)", error, strerror(error));

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	if (rump_sys_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
	    &tv, sizeof(tv), &error) == -1)
		errx(1, "setsockopt %d (%s)", error, strerror(error));

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(DEST_PORT);
	sin.sin_addr.s_addr = inet_addr(DEST_ADDR);

	if (rump_sys_connect(s, (struct sockaddr *)&sin, sizeof(sin),
	    &error) == -1) {
		errx(1, "connect failed: %d (%s)", error, strerror(error));
	}

	printf("connected\n");

	strcpy(buf, "GET / HTTP/1.0\n\n");
	n = rump_sys_write(s, buf, strlen(buf), &error);
	if (n != strlen(buf))
		errx(1, "wrote only %zd vs. %zu (%s)\n",
		    n, strlen(buf), strerror(error));
	
	memset(buf, 0, sizeof(buf));
	for (off = 0; off < sizeof(buf) && n > 0;) {
		n = rump_sys_read(s, buf+off, sizeof(buf)-off, &error);
		if (n > 0)
			off += n;
	}
	printf("read %zd (max %zu):\n", off, sizeof(buf));
	printf("%s", buf);

	return 0;
}

/*	$NetBSD: srec.c,v 1.1.30.1 2000/11/20 20:15:31 bouyer Exp $	*/

/*
 * Public domain, believed to be by Mike Price.
 * 
 * convert binary file to Srecord format
 * XXX srec generates improper checksums for 4-byte dumps
 */
#include <stdio.h>
#include <ctype.h>

int get32(char *);
void put32(int, int, char *, int, int);
void sput(char *);
void put(int);
int checksum(int, char *, int, int);
int main(int, char *[]);

int mask;
int size;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char buf[32];
	int cc;
	int base;
	int addr;
	char *name;

	if (argc != 4) {
		fprintf(stderr, "usage: %s {size} {hex_addr} {name}\n", argv[0]);
		fprintf(stderr, "Size = 2, 3, or 4 byte address\n");
		exit(1);
	}
	sscanf(argv[1], "%x", &size);
	mask = (1 << (size * 8)) - 1;
	if (!mask)
		mask = (-1);
	sscanf(argv[2], "%x", &base);
	name = argv[3];

	if (size == 2)
		printf("S0%02X%04X", 2 + strlen(name) + 1, 0);
	if (size == 3)
		printf("S0%02X%06X", 3 + strlen(name) + 1, 0);
	if (size == 4)
		printf("S0%02X%08X", 4 + strlen(name) + 1, 0);
	sput(name);
	printf("%02X\n", checksum(0, name, strlen(name), size));

	addr = base;
	for (;;) {
		cc = get32(buf);
		if (cc > 0) {
			put32(cc, addr, buf, size, mask);
			addr += cc;
		} else
			break;
	}

	buf[0] = base >> 8;
	buf[1] = base;
	printf("S%d%02X", 11 - size, 2 + 1);
	switch (size) {
	case 2:
		printf("%04X", base & mask);
		break;
	case 3:
		printf("%06X", base & mask);
		break;
	case 4:
		printf("%08X", base & mask);
		break;
	}

	/*
	 * kludge -> don't know why you have to add the +1 = works
	 * for size =3 at least
	 */
	printf("%02X\n", checksum(base, (char *) 0, 0, size) + 1);
	exit (0);
}

int
get32(buf)
	char buf[];
{
	char *cp = buf;
	int i;
	int c;

	for (i = 0; i < 32; ++i) {
		if ((c = getchar()) != EOF)
			*cp++ = c;
		else
			break;
	}
	return (cp - buf);
}

void
put32(len, addr, buf, size, mask)
	int len;
	int addr;
	char buf[];
	int size, mask;
{
	char *cp = buf;
	int i;

	if (size == 2)
		printf("S1%02X%04X", 2 + len + 1, addr & mask);
	if (size == 3)
		printf("S2%02X%06X", 3 + len + 1, addr & mask);
	if (size == 4)
		printf("S3%02X%08X", 4 + len + 1, addr & mask);
	for (i = 0; i < len; ++i)
		put(*cp++);
	printf("%02X\n", checksum(addr, buf, len, size));
}

void
sput(s)
	char *s;
{
	while (*s != '\0')
		put(*s++);
}

void
put(c)
	int c;
{
	printf("%02X", c & 0xff);
}

int
checksum(addr, buf, len, size)
	int addr;
	char buf[];
	int len;
	int size;
{
	char *cp = buf;
	int sum = 0xff - 1 - size - (len & 0xff);
	int i;

	if (size == 4)
		sum -= (addr >> 24) & 0xff;
	if (size >= 3)
		sum -= (addr >> 16) & 0xff;
	sum -= (addr >> 8) & 0xff;
	sum -= addr & 0xff;
	for (i = 0; i < len; ++i) {
		sum -= *cp++ & 0xff;
	}
	return (sum & 0xff);
}

/*	$NetBSD: loadkmap.c,v 1.2.50.1 2002/04/17 00:02:45 nathanw Exp $	*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "../../dev/iteioctl.h"
#include "../../dev/kbdmap.h"
#include <stdio.h>


int load_kmap __P((const char *, int));
int dump_kmap(); 

int
main(argc, argv)
     int argc;
     char *argv[];
{
	int	set_sysmap = 0;
	char	*mapfile;
	int	rc = 0;

	if (argc > 2) {
		if ((argc == 3) && !strcmp(argv[1], "-f")) {
			mapfile = argv[2];
			set_sysmap = 1;
		}
		else {
			fprintf(stderr, "%s [-f] keymap\n", argv[0]);
			exit(1);
		}
	}
	else mapfile = argv[1];

	if (argc == 1)
		rc = dump_kmap();
	else rc = load_kmap(mapfile, set_sysmap);

	exit (rc);
}


int
load_kmap(file, set_sysmap)
const char	*file;
int		set_sysmap;
{
	int	fd;
	char	buf[sizeof (struct kbdmap)];
	int	ioc;

	ioc = set_sysmap ? ITEIOCSSKMAP : ITEIOCSKMAP;
	
	if ((fd = open (file, 0)) >= 0) {
		if (read (fd, buf, sizeof (buf)) == sizeof (buf)) {
			if (ioctl (0, ioc, buf) == 0) {
				close(fd);
				return 0;
			}
			else perror("ITEIOCSKMAP");
		}
		else perror("read kmap");

		close(fd);
	}
	else perror("open kmap");
	return 1;
}

int
dump_kmap()
{
	char buf[sizeof (struct kbdmap)];

	if (ioctl (0, ITEIOCGKMAP, buf) == 0) {
		write (1, buf, sizeof (buf));
		return 0;
	}
	perror ("ITEIOCGKMAP");
	return 1;
}

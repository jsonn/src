#	$NetBSD: RunList.awk,v 1.1.1.1.6.1 1996/08/29 03:10:58 gwr Exp $

BEGIN {
	printf("cd ${CURDIR}\n");
	printf("\n");
}
/^$/ || /^#/ {
	print $0;
	next;
}
$1 == "COPY" {
	printf("echo '%s'\n", $0);
	printf("cp %s ${TARGDIR}/%s\n", $2, $3);
	next;
}
$1 == "LINK" {
	printf("echo '%s'\n", $0);
	printf("(cd ${TARGDIR}; ln %s %s)\n", $2, $3);
	next;
}
$1 == "SYMLINK" {
	printf("echo '%s'\n", $0);
	printf("(cd ${TARGDIR}; rm -f %s; ln -s %s %s)\n", $3, $2, $3);
	next;
}
$1 == "SPECIAL" {
	printf("echo '%s'\n", $0);
	printf("(cd ${TARGDIR};");
	for (i = 2; i <= NF; i++)
		printf(" %s", $i);
	printf(")\n");
	next;
}
{
	printf("echo '%s'\n", $0);
	printf("echo 'Unknown keyword \"%s\" at line %d of input.'\n", $1, NR);
	printf("exit 1\n");
	exit 1;
}
END {
	printf("\n");
	printf("exit 0\n");
	exit 0;
}

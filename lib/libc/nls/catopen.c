/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#include <sys/cdefs.h>
#include <nl_types.h>

extern nl_catd _catopen __P((__const char *, int));

nl_catd
catopen(name, oflag)
	__const char *name;
	int oflag;
{
	return _catopen(name, oflag);
}

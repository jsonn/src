/*
 * Written by Gordon W. Ross <gwr@netbsd.org>
 * Public domain.
 */

#include <sys/localedef.h>
#include <locale.h>

/*
 * Cheap and dirty setlocale() that is just good enough to
 * satisfy references in programs like cat that do:
 *		setlocale(LC_ALL, "");
 * Offered with apologies to all non-english speakers...
 */

static char current_locale[32] = { "C" };

char *
setlocale(category, locale)
	int category;
	const char *locale;
{
	if (category < 0 || category >= _LC_LAST)
		return (NULL);

	/* No change of locale is allowed. */
	if (locale && locale[0])
		return(NULL);

	return (current_locale);
}

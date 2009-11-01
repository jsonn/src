/*	$NetBSD: xen_acpi_machdep.c,v 1.4.24.2 2009/11/01 13:58:47 jym Exp $	*/

#include "acpica.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xen_acpi_machdep.c,v 1.4.24.2 2009/11/01 13:58:47 jym Exp $");

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpivar.h>
#define ACPI_MACHDEP_PRIVATE
#include <machine/acpi_machdep.h>

int
acpi_md_sleep(int state)
{
	printf("acpi: sleep not implemented\n");
	return (-1);
}

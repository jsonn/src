/*	$NetBSD: md-static-funcs.c,v 1.1.2.2 1995/10/19 13:10:18 ragge Exp $	*/
/*
 * Called by ld.so when onanating.
 * This *must* be a static function, so it is not called through a jmpslot.
 */

static void
md_relocate_simple(r, relocation, addr)
struct relocation_info	*r;
long			relocation;
char			*addr;
{
if (r->r_relative)
	*(long *)addr += relocation;
}


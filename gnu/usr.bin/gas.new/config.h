/*	$NetBSD: config.h,v 1.3.2.2 1998/11/07 00:56:48 cgd Exp $
 *
 * Munged from actual 2.8.1 config.h file for machine independence.
 *
 * NOTE: Define BFD_ASSEMBLER, TARGET_ALIAS, TARGET_CPU, and
 * 	TARGET_CANONICAL must be defined elsewhere.
 */

/* config.h.  Generated automatically by make.  */
#ifndef GAS_VERSION
#define GAS_VERSION "2.8.1"

/* conf.  Generated automatically by configure.  */
/* conf.in.  Generated automatically from configure.in by autoheader.  */

/* Define if using alloca.c.  */
/* #undef C_ALLOCA */

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
/* #undef CRAY_STACKSEG_END */

/* Define if you have alloca, as a function or macro.  */
#define HAVE_ALLOCA 1

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
/* #undef HAVE_ALLOCA_H */

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
 STACK_DIRECTION > 0 => grows toward higher addresses
 STACK_DIRECTION < 0 => grows toward lower addresses
 STACK_DIRECTION = 0 => direction of growth unknown
 */
/* #undef STACK_DIRECTION */

/* Should gas use high-level BFD interfaces?  */
/* #undef BFD_ASSEMBLER */

/* Some assert/preprocessor combinations are incapable of handling
   certain kinds of constructs in the argument of assert.  For example,
   quoted strings (if requoting isn't done right) or newlines.  */
/* #undef BROKEN_ASSERT */

/* If we aren't doing cross-assembling, some operations can be optimized,
   since byte orders and value sizes don't need to be adjusted.  */
/* #undef CROSS_COMPILE */

/* Some gas code wants to know these parameters.  */
#if !defined(TARGET_ALIAS) || !defined(TARGET_CPU) || \
    !defined(TARGET_CANONICAL)
#error TARGET_ALIAS, TARGET_CPU, and TARGET_CANONICAL must all be defined
#endif
#define TARGET_OS "netbsd"
#define TARGET_VENDOR "unknown"

/* Sometimes the system header files don't declare strstr.  */
/* #undef NEED_DECLARATION_STRSTR */

/* Sometimes the system header files don't declare malloc and realloc.  */
/* #undef NEED_DECLARATION_MALLOC */

/* Sometimes the system header files don't declare free.  */
/* #undef NEED_DECLARATION_FREE */

/* Sometimes the system header files don't declare sbrk.  */
/* #undef NEED_DECLARATION_SBRK */

/* Sometimes errno.h doesn't declare errno itself.  */
/* #undef NEED_DECLARATION_ERRNO */

/* #undef MANY_SEGMENTS */

/* Needed only for sparc configuration.  */
/* #undef SPARC_V9 */
/* #undef SPARC_ARCH64 */

/* Defined if using CGEN.  */
/* #undef USING_CGEN */

/* Needed only for some configurations that can produce multiple output
   formats.  */
#ifndef DEFAULT_EMULATION
# define DEFAULT_EMULATION ""
#endif
#ifndef EMULATIONS
# define EMULATIONS 
#endif
/* #undef USE_EMULATIONS */
/* #undef OBJ_MAYBE_AOUT */
/* #undef OBJ_MAYBE_BOUT */
/* #undef OBJ_MAYBE_COFF */
/* #undef OBJ_MAYBE_ECOFF */
/* #undef OBJ_MAYBE_ELF */
/* #undef OBJ_MAYBE_GENERIC */
/* #undef OBJ_MAYBE_HP300 */
/* #undef OBJ_MAYBE_IEEE */
/* #undef OBJ_MAYBE_SOM */
/* #undef OBJ_MAYBE_VMS */

/* Used for some of the COFF configurations, when the COFF code needs
   to select something based on the CPU type before it knows it... */
/* #undef I386COFF */
/* #undef M68KCOFF */
/* #undef M88KCOFF */

/* Define if you have the remove function.  */
/* #undef HAVE_REMOVE */

/* Define if you have the sbrk function.  */
#define HAVE_SBRK 1

/* Define if you have the unlink function.  */
#define HAVE_UNLINK 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <varargs.h> header file.  */
#define HAVE_VARARGS_H 1
#endif /* GAS_VERSION */

/*	$NetBSD: patchlevel.h,v 1.11.4.1 2000/09/30 06:20:45 simonb Exp $	*/

#define	FILE_VERSION_MAJOR	3
#define	patchlevel		32

/*
 * Patchlevel file for Ian Darwin's MAGIC command.
 * Id: patchlevel.h,v 1.32 2000/08/05 18:24:18 christos Exp 
 *
 * Log: patchlevel.h,v 
 * Revision 1.32  2000/08/05 18:24:18  christos
 * Correct indianness detection in elf (Charles Hannum)
 * FreeBSD elf core support (Guy Harris)
 * Use gzip in systems that don't have uncompress (Anthon van der Neut)
 * Internationalization/EBCDIC support (Eric Fisher)
 * Many many magic changes
 *
 * Revision 1.31  2000/05/14 17:58:36  christos
 * - new magic for claris files
 * - new magic for mathematica and maple files
 * - new magic for msvc files
 * - new -k flag to keep going matching all possible entries
 * - add the word executable on #! magic files, and fix the usage of
 *   the word script
 * - lots of other magic fixes
 * - fix typo test -> text
 *
 * Revision 1.30  2000/04/11 02:41:17  christos
 * - add support for mime output (-i)
 * - make sure we free memory in case realloc fails
 * - magic fixes
 *
 * Revision 1.29  1999/11/28 20:02:29  christos
 * new string/[Bcb] magic from anthon, and adjustments to the magic files to
 * use it.
 *
 * Revision 1.28  1999/10/31 22:11:48  christos
 * - add "char" type for compatibility with HP/UX
 * - recognize HP/UX syntax &=n etc.
 * - include errno.h for CYGWIN
 * - conditionalize the S_IS* macros
 * - revert the SHT_DYNSYM test that broke the linux stripped binaries test
 * - lots of Magdir changes
 *
 * Revision 1.27  1999/02/14 17:21:41  christos
 * Automake support and misc cleanups from Rainer Orth
 * Enable reading character and block special files from Dale R. Worley
 *
 * Revision 1.26  1998/09/12 13:19:39  christos
 * - add support for bi-endian indirect offsets (Richard Verhoeven)
 * - add recognition for bcpl (Joseph Myers)
 * - remove non magic files from Magdir to avoid difficulties building
 *   on os2 where files are case independent
 * - magic fixes.
 *
 * Revision 1.25  1998/06/27 14:04:04  christos
 * OLF patch Guy Harris
 * Recognize java/html (debian linux)
 * Const poisoning (debian linux)
 * More magic!
 *
 * Revision 1.24  1998/02/15 23:20:38  christos
 * Autoconf patch: Felix von Leitner <leitner@math.fu-berlin.de>
 * More magic fixes
 * Elf64 fixes
 *
 * Revision 1.23  1997/11/05 16:03:37  christos
 * - correct elf prps offset for SunOS-2.5.1 [guy@netapp.com]
 * - handle 64 bit time_t's correctly [ewt@redhat.com]
 * - new mime style magic [clarosse@netvista.net]
 * - new TI calculator magic [rmcguire@freenet.columbus.oh.us]
 * - new figlet fonts [obrien@freebsd.org]
 * - new cisco magic, and elf fixes [jhawk@bbnplanet.com]
 * - -b flag addition, and x86 filesystem magic [vax@linkhead.paranoia.com]
 * - s/Mpeg/MPEG, header and elf typo fixes [guy@netapp.com]
 * - Windows/NT registry files, audio code [guy@netapp.com]
 * - libGrx graphics lib fonts [guy@netapp.com]
 * - PNG fixes [guy@netapp.com]
 * - more m$ document magic [guy@netapp.com]
 * - PPD files [guy@netapp.com]
 * - archive magic cleanup [guy@netapp.com]
 * - linux kernel magic cleanup [guy@netapp.com]
 * - lecter magic [guy@netapp.com]
 * - vgetty magic [guy@netapp.com]
 * - sniffer additions [guy@netapp.com]
 *
 * Revision 1.22  1997/01/15 17:23:24  christos
 * - add support for elf core files: find the program name under SVR4 [Ken Pizzini]
 * - print strings only up to the first carriage return [various]
 * - freebsd international ascii support [J Wunsch]
 * - magic fixes and additions [Guy Harris]
 * - 64 bit fixes [Larry Schwimmer]
 * - support for both utime and utimes, but don't restore file access times
 *   by default [various]
 * - \xXX only takes 2 hex digits, not 3.
 * - re-implement support for core files [Guy Harris]
 *
 * Revision 1.21  1996/10/05 18:15:29  christos
 * Segregate elf stuff and conditionally enable it with -DBUILTIN_ELF
 * More magic fixes
 *
 * Revision 1.20  1996/06/22  22:15:52  christos
 * - support relative offsets of the form >&
 * - fix bug with truncating magic strings that contain \n
 * - file -f - did not read from stdin as documented
 * - support elf file parsing using our own elf support.
 * - as always magdir fixes and additions.
 *
 * Revision 1.19  1995/10/27  23:14:46  christos
 * Ability to parse colon separated list of magic files
 * New LEGAL.NOTICE
 * Various magic file changes
 *
 * Revision 1.18  1995/05/20  22:09:21  christos
 * Passed incorrect argument to eatsize().
 * Use %ld and %lx where appropriate.
 * Remove unused variables
 * ELF support for both big and little endian
 * Fixes for small files again.
 *
 * Revision 1.17  1995/04/28  17:29:13  christos
 * - Incorrect nroff detection fix from der Mouse
 * - Lost and incorrect magic entries.
 * - Added ELF stripped binary detection [in C; ugh]
 * - Look for $MAGIC to find the magic file.
 * - Eat trailing size specifications from numbers i.e. ignore 10L
 * - More fixes for very short files
 *
 * Revision 1.16  1995/03/25  22:06:45  christos
 * - use strtoul() where it exists.
 * - fix sign-extend bug
 * - try to detect tar archives before nroff files, otherwise
 *   tar files where the first file starts with a . will not work
 *
 * Revision 1.15  1995/01/21  21:03:35  christos
 * Added CSECTION for the file man page
 * Added version flag -v
 * Fixed bug with -f input flag (from iorio@violet.berkeley.edu)
 * Lots of magic fixes and reorganization...
 *
 * Revision 1.14  1994/05/03  17:58:23  christos
 * changes from mycroft@gnu.ai.mit.edu (Charles Hannum) for unsigned
 *
 * Revision 1.13  1994/01/21  01:27:01  christos
 * Fixed null termination bug from Don Seeley at BSDI in ascmagic.c
 *
 * Revision 1.12  1993/10/27  20:59:05  christos
 * Changed -z flag to understand gzip format too.
 * Moved builtin compression detection to a table, and move
 * the compress magic entry out of the source.
 * Made printing of numbers unsigned, and added the mask to it.
 * Changed the buffer size to 8k, because gzip will refuse to
 * unzip just a few bytes.
 *
 * Revision 1.11  1993/09/24  18:49:06  christos
 * Fixed small bug in softmagic.c introduced by
 * copying the data to be examined out of the input
 * buffer. Changed the Makefile to use sed to create
 * the correct man pages.
 *
 * Revision 1.10  1993/09/23  21:56:23  christos
 * Passed purify. Fixed indirections. Fixed byte order printing.
 * Fixed segmentation faults caused by referencing past the end
 * of the magic buffer. Fixed bus errors caused by referencing
 * unaligned shorts or longs.
 *
 * Revision 1.9  1993/03/24  14:23:40  ian
 * Batch of minor changes from several contributors.
 *
 * Revision 1.8  93/02/19  15:01:26  ian
 * Numerous changes from Guy Harris too numerous to mention but including
 * byte-order independance, fixing "old-style masking", etc. etc. A bugfix
 * for broken symlinks from martin@@d255s004.zfe.siemens.de.
 * 
 * Revision 1.7  93/01/05  14:57:27  ian
 * Couple of nits picked by Christos (again, thanks).
 * 
 * Revision 1.6  93/01/05  13:51:09  ian
 * Lotsa work on the Magic directory.
 * 
 * Revision 1.5  92/09/14  14:54:51  ian
 * Fix a tiny null-pointer bug in previous fix for tar archive + uncompress.
 * 
 */


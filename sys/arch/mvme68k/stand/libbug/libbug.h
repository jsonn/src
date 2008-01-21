/*	$NetBSD: libbug.h,v 1.3.40.1 2008/01/21 09:37:48 yamt Exp $	*/

/*
 * prototypes and such.   note that get/put char are in stand.h
 */


void	mvmeprom_delay(int);
int	mvmeprom_diskrd(struct mvmeprom_dskio *);
int	mvmeprom_diskwr(struct mvmeprom_dskio *);
struct	mvmeprom_brdid *mvmeprom_getbrdid(void);
int	peekchar(void);
void	mvmeprom_outln(char *, char *);
void	mvmeprom_outstr(char *, char *);
void	mvmeprom_rtc_rd(struct mvmeprom_time *);

/*
 * bugcrt stuff 
 */

extern struct mvmeprom_args bugargs;

void	_bugstart(void);
void	bugexec(void (*)(void));

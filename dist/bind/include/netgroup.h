/*	$NetBSD: netgroup.h,v 1.1.1.1.4.2 2002/07/01 17:15:00 he Exp $	*/

#ifndef netgroup_h
#define netgroup_h

int getnetgrent(const char **machinep, const char **userp,
		const char **domainp);

int getnetgrent_r(char **machinep, char **userp, char **domainp,
		  char *buffer, int buflen);

void setnetgrent(const char *netgroup);

void endnetgrent(void);

int innetgr(const char *netgroup, const char *machine,
	    const char *user, const char *domain);

#endif

/*	$NetBSD: db_ixfr.c,v 1.1.1.1.2.2 1999/12/04 16:53:29 he Exp $	*/

#if !defined(lint) && !defined(SABER)
static char     rcsid[] = "Id: db_ixfr.c,v 8.18 1999/10/15 19:48:57 vixie Exp";
#endif                /* not lint */

/*
 * Portions Copyright (c) 1999 by Check Point Software Technologies, Inc.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Check Point Software Technologies Incorporated not be used 
 * in advertising or publicity pertaining to distribution of the document 
 * or software without specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND CHECK POINT SOFTWARE TECHNOLOGIES 
 * INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.   
 * IN NO EVENT SHALL CHECK POINT SOFTWARE TECHNOLOGIES INCORPRATED
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR 
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Manage ixfr transaction log
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <res_update.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include "port_after.h"

#include "named.h"

#define DBIXFR_ERROR            -1
#define DBIXFR_FOUND_RR         2
#define DBIXFR_END              3

static int ixfr_getrr(struct zoneinfo *, FILE *, const char *, char *,
		      ns_updrec **, u_int32_t *, u_int32_t *);

ns_updrec *
ixfr_get_change_list(struct zoneinfo *zp,
		     u_int32_t from_serial, u_int32_t to_serial)
{
	FILE *		fp;
	u_int32_t	old_serial, new_serial;
	char		origin[MAXDNAME];
	struct namebuf *np, *listnp, *finlistnp;
	LIST(ns_updrec)	listuprec;
	int		ret, mode;
	ns_updrec	*uprec;

	if (SEQ_GT(from_serial, to_serial))
		return (NULL);
	listnp = finlistnp = NULL;
	INIT_LIST(listuprec);
	if ((fp = fopen(zp->z_ixfr_base, "r")) == NULL) {
		ns_warning(ns_log_db, "%s: %s",
			   zp->z_ixfr_base, strerror(errno));
		return (NULL);
	}
	strcpy(origin, zp->z_origin);
	lineno = 1;
	np = NULL;
	mode = 0;
	old_serial = new_serial = 0;
	for (;;) {
		ret = ixfr_getrr(zp, fp, zp->z_ixfr_base, origin, &uprec,
				 &old_serial, &new_serial);
		switch (ret) {
		case DBIXFR_ERROR:
			(void) my_fclose(fp);
			ns_warning(ns_log_db, "Logical error in %s line %d", 
				   zp->z_ixfr_base, lineno);
			return (NULL);
		case DBIXFR_FOUND_RR:
			if (EMPTY(listuprec)) {
				/* skip updates prior to the one we want */
				if (uprec->r_zone != from_serial) {
					while (uprec != NULL) {
						ns_updrec *prev;

						if (uprec->r_dp != NULL)
						      db_freedata(uprec->r_dp);
						uprec->r_dp = NULL;
						prev = PREV(uprec, r_link);
						res_freeupdrec(uprec);
						uprec = prev;
					}
					break;
				}
			}
			APPEND(listuprec, uprec, r_link);
			/* continue; */
			break;
		case DBIXFR_END:
			(void) my_fclose(fp);
			return (HEAD(listuprec));
		default:
			(void) my_fclose(fp);
			return (NULL);
		}
	}
}

/*
 * int ixfr_have_log(struct zoneinfo *zp,u_int32_t from_serial,
 *                   u_int32_t to_serial)
 * 
 * verify that ixfr transaction log contains changes  
 * from from_serial to to_serial
 * 
 * returns: 
 *         0 = serial number is up to date
 *         1 = transision is possible 
 *        -1 = error while opening the ixfr transaction log
 *        -2 = error in parameters
 *        -3 = logical error in the history file 
 */
int
ixfr_have_log(struct zoneinfo *zp, u_int32_t from_serial, u_int32_t to_serial)
{
	FILE           *fp;
	u_int32_t       old_serial = 0, new_serial = 0;
	char            buf[BUFSIZ];
	char           *cp;
	struct stat     st;
	int             nonempty_lineno = -1, prev_pktdone = 0, cont = 0,
			inside_next = 0;
	int             err;
	int             id, rcode = NOERROR;

	if (SEQ_GT(from_serial, to_serial))
		return (-2);
	if (from_serial == to_serial)
		return (0);
	/* If there is no log file, just return. */
	if (zp->z_ixfr_base == NULL || zp->z_updatelog == NULL)
		return (-1);
	if (stat(zp->z_ixfr_base, &st) < 0) {
		if (errno != ENOENT)
			ns_error(ns_log_db,
				 "unexpected stat(%s) failure: %s",
				 zp->z_ixfr_base, strerror(errno));
		return (-1);
	}
	if ((fp = fopen(zp->z_ixfr_base, "r")) == NULL) {
		ns_warning(ns_log_db, "%s: %s",
			   zp->z_ixfr_base, strerror(errno));
		return (-1);
	}
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		ns_error(ns_log_update, "fgets() from %s failed: %s",
			 zp->z_updatelog, strerror(errno));
		fclose(fp);
		return (-1);
	}
	if (strcmp(buf, LogSignature) != 0) {
		ns_error(ns_log_update, "invalid log file %s",
			 zp->z_updatelog);
		fclose(fp);
		return (-3);
	}
	lineno = 1;
	for (;;) {
		if (getword(buf, sizeof buf, fp, 0)) {
			nonempty_lineno = lineno;
		} else {
			if (lineno == (nonempty_lineno + 1))
				continue;
			inside_next = 0;
			prev_pktdone = 1;
			cont = 1;
		}
		if (!strcasecmp(buf, "[DYNAMIC_UPDATE]") ||
		    !strcasecmp(buf, "[IXFR_UPDATE]")) {
			err = 0;
			rcode = NOERROR;
			cp = fgets(buf, sizeof buf, fp);
			if (cp != NULL)
				lineno++;
			if (cp == NULL || !sscanf((char *) cp, "id %d", &id))
				id = -1;
			inside_next = 1;
			prev_pktdone = 1;
			cont = 1;
		} else if (!strcasecmp(buf, "serial")) {
			cp = fgets(buf, sizeof buf, fp);
			if (cp != NULL)
				lineno++;
			if (sscanf((char *) cp, "%u", &old_serial)) {
				if (from_serial >= old_serial) {
					fclose(fp);
					return (1);
				} else {
					fclose(fp);
					return (-1);
				}
			}
			prev_pktdone = 1;
			cont = 1;
		} else if (!strcasecmp(buf, "[INCR_SERIAL]")) {
			/* XXXRTH not enough error checking here */
			cp = fgets(buf, sizeof buf, fp);
			if (cp != NULL)
				lineno++;
			if (cp == NULL ||
			    sscanf((char *) cp, "from %u to %u",
				    &old_serial, &new_serial) != 2) {
				fclose(fp);
				return (-3);
			} else if (from_serial >= old_serial) {
				fclose(fp);
				return (1);
			}
			fclose(fp);
			return (-1);
		}
		if (prev_pktdone) {
			prev_pktdone = 0;
			if (feof(fp))
				break;
		}
	}
	fclose(fp);
	return (0);
}

/* from db_load.c */

static struct map m_section[] = {
	{"zone", S_ZONE},
	{"prereq", S_PREREQ},
	{"update", S_UPDATE},
	{"reserved", S_ADDT},
};
#define M_SECTION_CNT (sizeof(m_section) / sizeof(struct map))

/* from ns_req.c */

static struct map m_opcode[] = {
	{"nxdomain", NXDOMAIN},
	{"yxdomain", YXDOMAIN},
	{"nxrrset", NXRRSET},
	{"yxrrset", YXRRSET},
	{"delete", DELETE},
	{"add", ADD},
};
#define M_OPCODE_CNT (sizeof(m_opcode) / sizeof(struct map))

/* XXXRTH workaround map difficulties */
#define M_CLASS_CNT m_class_cnt
#define M_TYPE_CNT m_type_cnt

/*
 * int
 * ixfr_getrr(struct zoneinfo *zp, FILE *fp,
 *            const char *filename, char *origin, struct namebuf **np,
 *            u_int32_t *old_serial, u_int32_t *new_serial)
 * 
 * read a line from the historic of a zone.
 * 
 * returns:
 * 
 *         DBIXFR_ERROR = an error occured 
 *         DBIXFR_FOUND_RR = a rr encountered 
 *         DBIXFR_END = end of file
 */
static int
ixfr_getrr(struct zoneinfo *zp, FILE *fp, const char *filename, char *origin,
	   ns_updrec **uprec, u_int32_t *old_serial,
	   u_int32_t *new_serial)
{
	static int      read_soa, read_ns, rrcount;

	char            data[MAXDATA], dnbuf[MAXDNAME], sclass[3];
	const char     *errtype = "Database";
	char           *dname, *cp, *cp1;
	char            buf[MAXDATA];
	u_int32_t       serial, ttl;
	int             nonempty_lineno = -1, prev_pktdone = 0, cont = 0,
			inside_next = 0;
	int             id, rcode = NOERROR;
	int             i, c, section, opcode, matches, zonenum, err, multiline;
	int             type, class;
	u_int32_t       n;
	enum transport  transport;
	struct map     *mp;
	int             zonelist[MAXDNAME];
	struct databuf *dp;
	struct in_addr  ina;
	struct sockaddr_in empty_from;
	int             datasize;
	ns_updque	listuprec;
	ns_updrec *	rrecp;
	u_long		l;

#define    ERRTO(msg)  if (1) { errtype = msg; goto err; } else (void)NULL

	err = 0;
	transport = primary_trans;
	lineno = 1;
	INIT_LIST(listuprec);
	for (;;) {
		if (!getword(buf, sizeof buf, fp, 0)) {
			if (lineno == (nonempty_lineno + 1) && !(feof(fp))) {
				/*
				 * End of a nonempty line inside an update
				 * packet or not inside an update packet.
				 */
				continue;
			}
			/*
			 * Empty line or EOF.
			 * 
			 * Marks completion of current update packet.
			 */
			inside_next = 0;
			prev_pktdone = 1;
			cont = 1;
		} else {
			nonempty_lineno = lineno;
		}

		if (!strcasecmp(buf, "[DYNAMIC_UPDATE]") ||
		    !strcasecmp(buf, "[IXFR_UPDATE]")) {
			err = 0;
			rcode = NOERROR;
			cp = fgets(buf, sizeof buf, fp);
			if (cp != NULL)
				lineno++;
			if (cp == NULL || !sscanf((char *) cp, "id %d", &id))
				id = -1;
			inside_next = 1;
			prev_pktdone = 1;
			cont = 1;
		} else if (!strcasecmp(buf, "[INCR_SERIAL]")) {
			/* XXXRTH not enough error checking here */
			cp = fgets(buf, sizeof buf, fp);
			if (cp != NULL)
				lineno++;
			if (cp == NULL ||
			    sscanf((char *) cp, "from %u to %u",
				    old_serial, new_serial) != 2) {
				ns_error(ns_log_update,
					 "incr_serial problem with %s",
					 zp->z_updatelog);
			} else {
				serial = get_serial(zp);
			}
			cont = 1;
		} else if (!strcasecmp(buf, "[END_DELTA]")) {
			prev_pktdone = 1;
			cont = 1;
			lineno++;
		}
		if (prev_pktdone) {
			if (!EMPTY(listuprec)) {
				n++;
				*uprec = TAIL(listuprec);
				return (DBIXFR_FOUND_RR);
			}
			prev_pktdone = 0;
			if (feof(fp))
				break;
		}
		if (cont) {
			cont = 0;
			continue;
		}
		if (!inside_next)
			continue;
		/*
		 * inside the same update packet, continue accumulating
		 * records.
		 */
		section = -1;
		n = strlen(buf);
		if (buf[n - 1] == ':')
			buf[--n] = '\0';
		for (mp = m_section; mp < m_section + M_SECTION_CNT; mp++)
			if (!strcasecmp(buf, mp->token)) {
				section = mp->val;
				break;
			}
		ttl = 0;
		type = -1;
		class = zp->z_class;
		n = 0;
		data[0] = '\0';
		switch (section) {
		case S_ZONE:
			cp = fgets(buf, sizeof buf, fp);
			if (!cp)
				*buf = '\0';
			n = sscanf(cp, "origin %s class %s serial %ul",
				   origin, sclass, &serial);
			if (n != 3 || ns_samename(origin, zp->z_origin) != 1)
				err++;
			if (cp)
				lineno++;
			if (!err && inside_next) {
				int             success;

				dname = origin;
				type = T_SOA;
				class = sym_ston(__p_class_syms, sclass,
						 &success);
				if (!success) {
					err++;
					break;
				}
				matches = findzone(dname, class, 0,
						   zonelist, MAXDNAME);
				if (matches)
					zonenum = zonelist[0];
				else
					err++;
			}
			break;
		case S_PREREQ:
		case S_UPDATE:
			/* Operation code. */
			if (!getword(buf, sizeof buf, fp, 0)) {
				err++;
				break;
			}
			opcode = -1;
			if (buf[0] == '{') {
				n = strlen(buf);
				for (i = 0; (u_int32_t) i < n; i++)
					buf[i] = buf[i + 1];
				if (buf[n - 2] == '}')
					buf[n - 2] = '\0';
			}
			for (mp = m_opcode; mp < m_opcode + M_OPCODE_CNT; mp++)
				if (!strcasecmp(buf, mp->token)) {
					opcode = mp->val;
					break;
				}
			if (opcode == -1) {
				err++;
				break;
			}
			/* Owner's domain name. */
			if (!getword((char *) dnbuf, sizeof dnbuf, fp, 0)) {
				err++;
				break;
			}
			n = strlen((char *) dnbuf) - 1;
			if (dnbuf[n] == '.')
				dnbuf[n] = '\0';
			dname = dnbuf;
			ttl = 0;
			type = -1;
			class = zp->z_class;
			n = 0;
			data[0] = '\0';
			(void) getword(buf, sizeof buf, fp, 1);
			if (isdigit(buf[0])) {    /* ttl */
				if (ns_parse_ttl(buf, &l) < 0) {
					err++;
					break;
				}
				ttl = l;
				(void) getword(buf, sizeof buf, fp, 1);
			}
			/* possibly class */
			if (buf[0] != '\0') {
				int             success;
				int             maybe_class;

				maybe_class = sym_ston(__p_class_syms,
						       buf, &success);
				if (success) {
					class = maybe_class;
					(void) getword(buf, sizeof buf, fp, 1);
				}
			}
			/* possibly type */
			if (buf[0] != '\0') {
				int             success;
				int             maybe_type;

				maybe_type = sym_ston(__p_type_syms,
						      buf, &success);

				if (success) {
					type = maybe_type;
					(void) getword(buf, sizeof buf, fp, 1);
				}
			}
			if (buf[0] != '\0')    /* possibly rdata */
			/*
			 * Convert the ascii data 'buf' to the proper
			 * format based on the type and pack into
			 * 'data'.
			 * 
			 * XXX - same as in db_load(), consolidation
			 * needed
			 */
			switch (type) {
			case T_A:
				if (!inet_aton(buf, &ina)) {
					err++;
					break;
				}
				n = ntohl(ina.s_addr);
				cp = data;
				PUTLONG(n, cp);
				n = INT32SZ;
				break;
			case T_HINFO:
			case T_ISDN:
				n = strlen(buf);
				data[0] = n;
				memcpy(data + 1, buf, n);
				n++;
				if (!getword(buf, sizeof buf, fp, 0)) {
					i = 0;
				} else {
					endline(fp);
					i = strlen(buf);
				}
				data[n] = i;
				n++;
				memcpy(data + n + 1, buf, i);
				n += i;
				break;
			case T_SOA:
			case T_MINFO:
			case T_RP:
				(void) strcpy(data, buf);
				cp = data + strlen(data) + 1;
				if (!getword((char *) cp,
					     sizeof data - (cp - data),
					     fp, 1)) {
					err++;
					break;
				}
				cp += strlen((char *) cp) + 1;
				if (type != T_SOA) {
					n = cp - data;
					break;
				}
				if (class != zp->z_class ||
				    ns_samename(dname, zp->z_origin) != 1) {
					err++;
					break;
				}
				c = getnonblank(fp, zp->z_updatelog);
				if (c == '(') {
					multiline = 1;
				} else {
					multiline = 0;
					ungetc(c, fp);
				}
				n = getnum(fp, zp->z_updatelog, GETNUM_SERIAL);
				if (getnum_error) {
					err++;
					break;
				}
				if (opcode == ADD && i == 0)
					*new_serial = n;
				PUTLONG(n, cp);
				for (i = 0; i < 4; i++) {
					if (!getword(buf, sizeof buf, fp, 1)) {
						err++;
						break;
					}
					if (ns_parse_ttl(buf, &l) < 0) {
						err++;
						break;
					}
					n = l;
					PUTLONG(n, cp);
				}
				if (multiline &&
				    getnonblank(fp, zp->z_updatelog) != ')')
				{
					err++;
					break;
				}
				endline(fp);
				n = cp - data;
				break;
			case T_WKS:
				if (!inet_aton(buf, &ina)) {
					err++;
					break;
				}
				n = ntohl(ina.s_addr);
				cp = data;
				PUTLONG(n, cp);
				*cp = (char) getprotocol(fp, zp->z_updatelog);
				n = INT32SZ + sizeof(char);
				n = getservices((int) n, data,
						fp, zp->z_updatelog);
				break;
			case T_NS:
			case T_CNAME:
			case T_MB:
			case T_MG:
			case T_MR:
			case T_PTR:
				(void) strcpy(data, buf);
				if (makename(data, origin,
					     sizeof(data)) == -1) {
					err++;
					break;
				}
				n = strlen(data) + 1;
				break;
			case T_MX:
			case T_AFSDB:
			case T_RT:
				n = 0;
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				/* catch bad values */
				cp = data;
				PUTSHORT((u_int16_t) n, cp);
				if (!getword(buf, sizeof(buf), fp, 1)) {
					err++;
					break;
				}
				(void) strcpy((char *) cp, buf);
				if (makename((char *) cp, origin,
					     sizeof(data) - (cp - data)) == -1)
				{
					err++;
					break;
				}
				/* advance pointer to end of data */
				cp += strlen((char *) cp) + 1;
				/* now save length */
				n = (cp - data);
				break;
			case T_PX:
				n = 0;
				data[0] = '\0';
				cp = buf;
				while (isdigit(*cp))
					n = n * 10 + (*cp++ - '0');
				cp = data;
				PUTSHORT((u_int16_t) n, cp);
				for (i = 0; i < 2; i++) {
					if (!getword(buf, sizeof(buf), fp, 0))
					{
						err++;
						break;
					}
					(void) strcpy((char *) cp, buf);
					cp += strlen((char *) cp) + 1;
				}
				n = cp - data;
				break;
			case T_TXT:
			case T_X25:
				i = strlen(buf);
				cp = data;
				datasize = sizeof data;
				cp1 = buf;
				while (i > MAXCHARSTRING) {
					if (datasize <= MAXCHARSTRING) {
						ns_error(ns_log_update,
							 "record too big");
						return (-1);
					}
					datasize -= MAXCHARSTRING;
					*cp++ = (char)MAXCHARSTRING;
					memcpy(cp, cp1, MAXCHARSTRING);
					cp += MAXCHARSTRING;
					cp1 += MAXCHARSTRING;
					i -= MAXCHARSTRING;
				}
				if (datasize < i + 1) {
					ns_error(ns_log_update,
						 "record too big");
					return (-1);
				}
				*cp++ = i;
				memcpy(cp, cp1, i);
				cp += i;
				n = cp - data;
				endline(fp);
				/* XXXVIX: segmented texts 4.9.5 */
				break;
			case T_NSAP:
				n = inet_nsap_addr(buf, (u_char *) data,
						   sizeof data);
				endline(fp);
				break;
			case T_LOC:
				cp = buf + (n = strlen(buf));
				*cp = ' ';
				cp++;
				while ((i = getc(fp), *cp = i, i != EOF)
				       && *cp != '\n' && (n < MAXDATA))
				{
					cp++;
					n++;
				}
				if (*cp == '\n')
					ungetc(*cp, fp);
				*cp = '\0';
				n = loc_aton(buf, (u_char *) data);
				if (n == 0) {
					err++;
					break;
				}
				endline(fp);
				break;
			case ns_t_sig: 
			case ns_t_nxt:
			case ns_t_key:
			case ns_t_cert:{
				char *errmsg = NULL;

				n  = parse_sec_rdata(buf, sizeof(buf), 1,
						     (u_char *) data,
						     sizeof(data),
						     fp, zp, dname, ttl,
						     type, domain_ctx,
						     transport, &errmsg);
				if (errmsg) {
					err++;
					endline(fp);
					n = 0;
				}
				break;
			}
			default:
				err++;
			}
			if (section == S_PREREQ) {
				ttl = 0;
				if (opcode == NXDOMAIN) {
					class = C_NONE;
					type = T_ANY;
					n = 0;
				} else if (opcode == YXDOMAIN) {
					class = C_ANY;
					type = T_ANY;
					n = 0;
				} else if (opcode == NXRRSET) {
					class = C_NONE;
					n = 0;
				} else if (opcode == YXRRSET) {
					if (n == 0)
					class = C_ANY;
				}
			} else {/* section == S_UPDATE */
				if (opcode == DELETE) {
					if (n == 0) {
						class = C_ANY;
						if (type == -1)
						type = T_ANY;
					} else {
						class = zp->z_class;
					}
				}
			}
			break;
		case S_ADDT:
		default:
			ns_debug(ns_log_update, 1,
				 "cannot interpret section: %d", section);
			inside_next = 0;
			err++;
		}
		if (err) {
			inside_next = 0;
			ns_debug(ns_log_update, 1,
			"merge of update id %d failed due to error at line %d",
				 id, lineno);
			memset(&empty_from, 0, sizeof empty_from);
			free_rrecp(&listuprec, rcode, empty_from);
			continue;
		}
		rrecp = res_mkupdrec(section, dname, class, type, ttl);
		if (section != S_ZONE) {
			dp = savedata(class, type, ttl, (u_char *) data, n);
			dp->d_zone = zonenum;
			dp->d_cred = DB_C_ZONE;
			dp->d_clev = nlabels(zp->z_origin);
			rrecp->r_dp = dp;
			rrecp->r_opcode = opcode;
		} else {
			rrecp->r_zone = zonenum;
			rrecp->r_opcode = opcode;
		}

		/* remove add/delete pairs */
		if (section == S_UPDATE) {
			ns_updrec *arp;
			int foundmatch;

			arp = TAIL(listuprec);
			foundmatch = 0;
			while (arp) {
				if (arp->r_section == S_UPDATE &&
				    ((arp->r_opcode == DELETE &&
				      opcode == ADD) ||
				     (opcode == DELETE &&
				      arp->r_opcode == ADD)) &&
				     arp->r_dp->d_type == dp->d_type &&
				     arp->r_dp->d_class == dp->d_class &&
				     arp->r_dp->d_ttl == dp->d_ttl &&
				     ns_samename(arp->r_dname, dname) == 1 &&
				     db_cmp(arp->r_dp, dp) == 0) {
					db_freedata(dp);
					db_freedata(arp->r_dp);
					UNLINK(listuprec, arp, r_link);
					res_freeupdrec(arp);
					res_freeupdrec(rrecp);
					foundmatch = 1;
					break;
				}
				arp = PREV(arp, r_link);
			}
			if (foundmatch)
				continue;
		}

		APPEND(listuprec, rrecp, r_link);
		/* Override zone number with current zone serial number */
		rrecp->r_zone = serial;
	}   

	if (err)
		return (DBIXFR_ERROR);

	return (DBIXFR_END);
}

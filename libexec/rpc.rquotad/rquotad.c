/*	$NetBSD: rquotad.c,v 1.23.28.1 2009/05/13 19:18:43 jym Exp $	*/

/*
 * by Manuel Bouyer (bouyer@ensta.fr). Public domain.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: rquotad.c,v 1.23.28.1 2009/05/13 19:18:43 jym Exp $");
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>

#include <stdio.h>
#include <fstab.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <unistd.h>

#include <syslog.h>

#include <ufs/ufs/quota.h>
#include <rpc/rpc.h>
#include <rpcsvc/rquota.h>
#include <arpa/inet.h>

void rquota_service(struct svc_req *request, SVCXPRT *transp);
void ext_rquota_service(struct svc_req *request, SVCXPRT *transp);
void sendquota(struct svc_req *request, int vers, SVCXPRT *transp);
void initfs(void);
int getfsquota(int type, long id, char *path, struct dqblk *dqblk);
int hasquota(struct fstab *fs, char **uqfnamep, char **gqfnamep);
void cleanup(int);
int main(int, char *[]);

/*
 * structure containing informations about ufs filesystems
 * initialised by initfs()
 */
struct fs_stat {
	struct fs_stat *fs_next;	/* next element */
	char   *fs_file;		/* mount point of the filesystem */
	char   *uqfpathname;		/* pathname of the user quota file */
	char   *gqfpathname;		/* pathname of the group quota file */
	dev_t   st_dev;			/* device of the filesystem */
} fs_stat;
struct fs_stat *fs_begin = NULL;

const char *qfextension[] = INITQFNAMES;
int from_inetd = 1;

void 
cleanup(int dummy)
{

	(void)rpcb_unset(RQUOTAPROG, RQUOTAVERS, NULL);
	(void)rpcb_unset(RQUOTAPROG, EXT_RQUOTAVERS, NULL);
	exit(0);
}

int
main(int argc, char *argv[])
{
	SVCXPRT *transp;
	struct sockaddr_storage from;
	socklen_t fromlen;

	fromlen = sizeof(from);
	if (getsockname(0, (struct sockaddr *)&from, &fromlen) < 0)
		from_inetd = 0;

	if (!from_inetd) {
		daemon(0, 0);

		(void) rpcb_unset(RQUOTAPROG, RQUOTAVERS, NULL);
		(void) rpcb_unset(RQUOTAPROG, EXT_RQUOTAVERS, NULL);
		(void) signal(SIGINT, cleanup);
		(void) signal(SIGTERM, cleanup);
		(void) signal(SIGHUP, cleanup);
	}

	openlog("rpc.rquotad", LOG_PID, LOG_DAEMON);

	/* create and register the service */
	if (from_inetd) {
		transp = svc_dg_create(0, 0, 0);
		if (transp == NULL) {
			syslog(LOG_ERR, "couldn't create udp service.");
			exit(1);
		}
		if (!svc_reg(transp, RQUOTAPROG, RQUOTAVERS, rquota_service,
		    NULL)) {
			syslog(LOG_ERR,
			    "unable to register (RQUOTAPROG, RQUOTAVERS).");
			exit(1);
		}
		if (!svc_reg(transp, RQUOTAPROG, EXT_RQUOTAVERS,
		    ext_rquota_service, NULL)) {
			syslog(LOG_ERR,
			    "unable to register (RQUOTAPROG, EXT_RQUOTAVERS).");
			exit(1);
		}
	} else {
		if (!svc_create(rquota_service, RQUOTAPROG, RQUOTAVERS, "udp")){
			syslog(LOG_ERR,
			    "unable to create (RQUOTAPROG, RQUOTAVERS).");
			exit(1);
		}
		if (!svc_create(ext_rquota_service, RQUOTAPROG,
		    EXT_RQUOTAVERS, "udp")){
			syslog(LOG_ERR,
			    "unable to create (RQUOTAPROG, EXT_RQUOTAVERS).");
			exit(1);
		}
	}

	initfs();		/* init the fs_stat list */
	svc_run();
	syslog(LOG_ERR, "svc_run returned");
	exit(1);
}

void 
rquota_service(struct svc_req *request, SVCXPRT *transp)
{
	switch (request->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		break;

	case RQUOTAPROC_GETQUOTA:
	case RQUOTAPROC_GETACTIVEQUOTA:
		sendquota(request, RQUOTAVERS, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;
	}
	if (from_inetd)
		exit(0);
}

void 
ext_rquota_service(struct svc_req *request, SVCXPRT *transp)
{
	switch (request->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		break;

	case RQUOTAPROC_GETQUOTA:
	case RQUOTAPROC_GETACTIVEQUOTA:
		sendquota(request, EXT_RQUOTAVERS, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;
	}
	if (from_inetd)
		exit(0);
}

/* read quota for the specified id, and send it */
void 
sendquota(struct svc_req *request, int vers, SVCXPRT *transp)
{
	struct getquota_args getq_args;
	struct ext_getquota_args ext_getq_args;
	struct getquota_rslt getq_rslt;
	struct dqblk dqblk;
	struct timeval timev;

	memset((char *)&getq_args, 0, sizeof(getq_args));
	memset((char *)&ext_getq_args, 0, sizeof(ext_getq_args));
	switch (vers) {
	case RQUOTAVERS:
		if (!svc_getargs(transp, xdr_getquota_args,
		    (caddr_t)&getq_args)) {
			svcerr_decode(transp);
			return;
		}
		ext_getq_args.gqa_pathp = getq_args.gqa_pathp;
		ext_getq_args.gqa_id = getq_args.gqa_uid;
		ext_getq_args.gqa_type = RQUOTA_USRQUOTA;
		break;
	case EXT_RQUOTAVERS:
		if (!svc_getargs(transp, xdr_ext_getquota_args,
		    (caddr_t)&ext_getq_args)) {
			svcerr_decode(transp);
			return;
		}
		break;
	}
	if (request->rq_cred.oa_flavor != AUTH_UNIX) {
		/* bad auth */
		getq_rslt.status = Q_EPERM;
	} else if (!getfsquota(ext_getq_args.gqa_type, ext_getq_args.gqa_id,
	    ext_getq_args.gqa_pathp, &dqblk)) {
		/* failed, return noquota */
		getq_rslt.status = Q_NOQUOTA;
	} else {
		gettimeofday(&timev, NULL);
		getq_rslt.status = Q_OK;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_active = TRUE;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize = DEV_BSIZE;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bhardlimit =
		    dqblk.dqb_bhardlimit;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bsoftlimit =
		    dqblk.dqb_bsoftlimit;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_curblocks =
		    dqblk.dqb_curblocks;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_fhardlimit =
		    dqblk.dqb_ihardlimit;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_fsoftlimit =
		    dqblk.dqb_isoftlimit;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_curfiles =
		    dqblk.dqb_curinodes;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_btimeleft =
		    dqblk.dqb_btime - timev.tv_sec;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_ftimeleft =
		    dqblk.dqb_itime - timev.tv_sec;
	}
	if (!svc_sendreply(transp, xdr_getquota_rslt, (char *)&getq_rslt))
		svcerr_systemerr(transp);
	if (!svc_freeargs(transp, xdr_getquota_args, (caddr_t)&getq_args)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
}

/* initialise the fs_tab list from entries in /etc/fstab */
void 
initfs()
{
	struct fs_stat *fs_current = NULL;
	struct fs_stat *fs_next = NULL;
	char *uqfpathname, *gqfpathname;
	struct fstab *fs;
	struct stat st;

	setfsent();
	while ((fs = getfsent())) {
		if (strcmp(fs->fs_vfstype, MOUNT_FFS))
			continue;
		if (!hasquota(fs, &uqfpathname, &gqfpathname))
			continue;

		fs_current = (struct fs_stat *) malloc(sizeof(struct fs_stat));
		if (fs_current == NULL) {
			syslog(LOG_ERR, "can't malloc: %m");
			exit(1);
		}
		fs_current->fs_next = fs_next;	/* next element */

		fs_current->fs_file = strdup(fs->fs_file);
		if (fs_current->fs_file == NULL) {
			syslog(LOG_ERR, "can't strdup: %m");
			exit(1);
		}

		if (uqfpathname) {
			fs_current->uqfpathname = strdup(uqfpathname);
			if (fs_current->uqfpathname == NULL) {
				syslog(LOG_ERR, "can't strdup: %m");
				exit(1);
			}
		} else
			fs_current->uqfpathname = NULL;
		if (gqfpathname) {
			fs_current->gqfpathname = strdup(gqfpathname);
			if (fs_current->gqfpathname == NULL) {
				syslog(LOG_ERR, "can't strdup: %m");
				exit(1);
			}
		} else
			fs_current->gqfpathname = NULL;
		stat(fs->fs_file, &st);
		fs_current->st_dev = st.st_dev;

		fs_next = fs_current;
	}
	endfsent();
	fs_begin = fs_current;
}

/*
 * gets the quotas for id, filesystem path.
 * Return 0 if fail, 1 otherwise
 */
int
getfsquota(int type, long id, char *path, struct dqblk *dqblk)
{
	struct stat st_path;
	struct fs_stat *fs;
	int	qcmd, fd, ret = 0;
	char *filename;

	if (stat(path, &st_path) < 0)
		return (0);

	qcmd = QCMD(Q_GETQUOTA, type == RQUOTA_USRQUOTA ? USRQUOTA : GRPQUOTA);

	for (fs = fs_begin; fs != NULL; fs = fs->fs_next) {
		/* where the device is the same as path */
		if (fs->st_dev != st_path.st_dev)
			continue;

		/* find the specified filesystem. get and return quota */
		if (quotactl(fs->fs_file, qcmd, id, dqblk) == 0)
			return (1);
		filename = (type == RQUOTA_USRQUOTA) ?
		    fs->uqfpathname : fs->gqfpathname;
		if (filename == NULL)
			return 0;
		if ((fd = open(filename, O_RDONLY)) < 0) {
			syslog(LOG_WARNING, "open error: %s: %m", filename);
			return (0);
		}
		if (lseek(fd, (off_t)(id * sizeof(struct dqblk)), SEEK_SET)
		    == (off_t)-1) {
			close(fd);
			return (0);
		}
		switch (read(fd, dqblk, sizeof(struct dqblk))) {
		case 0:
			/*
                         * Convert implicit 0 quota (EOF)
                         * into an explicit one (zero'ed dqblk)
                         */
			memset((caddr_t) dqblk, 0, sizeof(struct dqblk));
			ret = 1;
			break;
		case sizeof(struct dqblk):	/* OK */
			ret = 1;
			break;
		default:	/* ERROR */
			syslog(LOG_WARNING, "read error: %s: %m", filename);
			close(fd);
			return (0);
		}
		close(fd);
	}
	return (ret);
}

/*
 * Check to see if a particular quota is to be enabled.
 * Comes from quota.c, NetBSD 0.9
 */
int
hasquota(struct fstab *fs, char **uqfnamep, char **gqfnamep)
{
	static char initname=0, usrname[100], grpname[100];
	static char buf[MAXPATHLEN], ubuf[MAXPATHLEN], gbuf[MAXPATHLEN];
	char	*opt, *cp = NULL;
	int ret = 0;

	if (!initname) {
		(void)snprintf(usrname, sizeof usrname, "%s%s",
		    qfextension[USRQUOTA], QUOTAFILENAME);
		(void)snprintf(grpname, sizeof grpname, "%s%s",
		    qfextension[GRPQUOTA], QUOTAFILENAME);
	}

	*uqfnamep = NULL;
	*gqfnamep = NULL;
	(void)strlcpy(buf, fs->fs_mntops, sizeof(buf));
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if ((cp = strchr(opt, '=')))
			*cp++ = '\0';
		if (strcmp(opt, usrname) == 0) {
			ret = 1;
			if (cp)
				*uqfnamep = cp;
			else {
				(void)snprintf(ubuf, sizeof ubuf, "%s/%s.%s",
				    fs->fs_file, QUOTAFILENAME,
				    qfextension[USRQUOTA]);
				*uqfnamep = ubuf;
			}
		} else if (strcmp(opt, grpname) == 0) {
			ret = 1;
			if (cp)
				*gqfnamep = cp;
			else {
				(void)snprintf(gbuf, sizeof gbuf, "%s/%s.%s",
				    fs->fs_file, QUOTAFILENAME,
				    qfextension[GRPQUOTA]);
				*gqfnamep = gbuf;
			}
		}
	}
	return (ret);
}

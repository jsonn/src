/*	$NetBSD: ev_connects.c,v 1.1.1.1 1998/10/05 18:01:58 tron Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997, 1998 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* ev_connects.c - implement asynch connect/accept for the eventlib
 * vix 16sep96 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "Id: ev_connects.c,v 8.19 1998/03/20 23:26:22 halley Exp";
#endif

/* Import. */

#include "port_before.h"
#include "fd_setsize.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/assertions.h>
#include "eventlib_p.h"

#include "port_after.h"

/* Forward. */

static void	listener(evContext ctx, void *uap, int fd, int evmask);
static void	connector(evContext ctx, void *uap, int fd, int evmask);

/* Public. */

int
evListen(evContext opaqueCtx, int fd, int maxconn,
	 evConnFunc func, void *uap, evConnID *id)
{
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *new;
	int mode;

	OKNEW(new);
	new->flags = EV_CONN_LISTEN;
	OK(mode = fcntl(fd, F_GETFL, NULL));	/* side effect: validate fd. */
	/*
	 * Remember the nonblocking status.  We assume that either evSelectFD
	 * has not been done to this fd, or that if it has then the caller
	 * will evCancelConn before they evDeselectFD.  If our assumptions
	 * are not met, then we might restore the old nonblocking status
	 * incorrectly.
	 */
	if ((mode & O_NONBLOCK) == 0) {
		OK(fcntl(fd, F_SETFL, mode | O_NONBLOCK));
		new->flags |= EV_CONN_BLOCK;
	}
	OK(listen(fd, maxconn));
	if (evSelectFD(opaqueCtx, fd, EV_READ, listener, new, &new->file) < 0){
		int save = errno;

		FREE(new);
		errno = save;
		return (-1);
	}
	new->flags |= EV_CONN_SELECTED;
	new->func = func;
	new->uap = uap;
	new->fd = fd;
	if (ctx->conns != NULL)
		ctx->conns->prev = new;
	new->prev = NULL;
	new->next = ctx->conns;
	ctx->conns = new;
	if (id)
		id->opaque = new;
	return (0);
}

int
evConnect(evContext opaqueCtx, int fd, void *ra, int ralen,
	  evConnFunc func, void *uap, evConnID *id)
{
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *new;

	OKNEW(new);
	new->flags = 0;
	/* Do the select() first to get the socket into nonblocking mode. */
	if (evSelectFD(opaqueCtx, fd, EV_MASK_ALL,
		       connector, new, &new->file) < 0) {
		int save = errno;

		FREE(new);
		errno = save;
		return (-1);
	}
	new->flags |= EV_CONN_SELECTED;
	if (connect(fd, ra, ralen) < 0 &&
	    errno != EWOULDBLOCK &&
	    errno != EAGAIN &&
	    errno != EINPROGRESS) {
		int save = errno;

		(void) evDeselectFD(opaqueCtx, new->file);
		FREE(new);
		errno = save;
		return (-1);
	}
	/* No error, or EWOULDBLOCK.  select() tells when it's ready. */
	new->func = func;
	new->uap = uap;
	new->fd = fd;
	if (ctx->conns != NULL)
		ctx->conns->prev = new;
	new->prev = NULL;
	new->next = ctx->conns;
	ctx->conns = new;
	if (id)
		id->opaque = new;
	return (0);
}

int
evCancelConn(evContext opaqueCtx, evConnID id) {
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *this = id.opaque;
	evAccept *acc, *nxtacc;
	int mode;

	if ((this->flags & EV_CONN_SELECTED) != 0)
		(void) evDeselectFD(opaqueCtx, this->file);
	if ((this->flags & EV_CONN_BLOCK) != 0) {
		mode = fcntl(this->fd, F_GETFL, NULL);
		if (mode == -1) {
			if (errno != EBADF)
				return (-1);
		} else
			OK(fcntl(this->fd, F_SETFL, mode | O_NONBLOCK));
	}
	
	/* Unlink from ctx->conns. */
	if (this->prev != NULL)
		this->prev->next = this->next;
	else
		ctx->conns = this->next;
	if (this->next != NULL)
		this->next->prev = this->prev;

	/*
	 * Remove `this' from the ctx->accepts list (zero or more times).
	 */
	for (acc = HEAD(ctx->accepts), nxtacc = NULL;
	     acc != NULL;
	     acc = nxtacc)
	{
		nxtacc = NEXT(acc, link);
		if (acc->conn == this) {
			UNLINK(ctx->accepts, acc, link);
			close(acc->fd);
			FREE(acc);
		}
	}

	/* Wrap up and get out. */
	FREE(this);
	return (0);
}

int evHold(evContext opaqueCtx, evConnID id) {
	evConn *this = id.opaque;

	if ((this->flags & EV_CONN_LISTEN) == 0) {
		errno = EINVAL;
		return (-1);
	}
	if ((this->flags & EV_CONN_SELECTED) == 0)
		return (0);
	this->flags &= ~EV_CONN_SELECTED;
	return (evDeselectFD(opaqueCtx, this->file));
}

int evUnhold(evContext opaqueCtx, evConnID id) {
	evConn *this = id.opaque;
	int ret;

	if ((this->flags & EV_CONN_LISTEN) == 0) {
		errno = EINVAL;
		return (-1);
	}
	if ((this->flags & EV_CONN_SELECTED) != 0)
		return (0);
	ret = evSelectFD(opaqueCtx, this->fd, EV_READ, listener, this,
			 &this->file);
	if (ret == 0)
		this->flags |= EV_CONN_SELECTED;
	return (ret);
}

int
evTryAccept(evContext opaqueCtx, evConnID id, int *sys_errno) {
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *conn = id.opaque;
	evAccept *new;

	if ((conn->flags & EV_CONN_LISTEN) == 0) {
		errno = EINVAL;
		return (-1);
	}
	OKNEW(new);
	new->conn = conn;
	new->ralen = sizeof new->ra;
	new->fd = accept(conn->fd, &new->ra, &new->ralen);
	if (new->fd >= 0) {
		new->lalen = sizeof new->la;
		if (getsockname(new->fd, &new->la, &new->lalen) < 0) {
			new->ioErrno = errno;
			(void) close(new->fd);
			new->fd = -1;
		} else
			new->ioErrno = 0;
	} else {
		new->ioErrno = errno;
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			FREE(new);
			return (-1);
		}
	}
	APPEND(ctx->accepts, new, link);
	*sys_errno = new->ioErrno;
	return (0);
}

/* Private. */

static void
listener(evContext opaqueCtx, void *uap, int fd, int evmask) {
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *conn = uap;
	struct sockaddr la, ra;
	int new, lalen, ralen;

	REQUIRE((evmask & EV_READ) != 0);
	ralen = sizeof ra;
	new = accept(fd, &ra, &ralen);
	if (new >= 0) {
		lalen = sizeof la;
		if (getsockname(new, &la, &lalen) < 0) {
			int save = errno;

			(void) close(new);
			errno = save;
			new = -1;
		}
	} else if (errno == EAGAIN || errno == EWOULDBLOCK)
		return;
	(*conn->func)(opaqueCtx, conn->uap, new, &la, lalen, &ra, ralen);
}

static void
connector(evContext opaqueCtx, void *uap, int fd, int evmask) {
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *conn = uap;
	struct sockaddr la, ra;
	int lalen, ralen;
	char buf[1];
	void *conn_uap;
	evConnFunc conn_func;
	evConnID id;
	int socket_errno = 0;
	int optlen;

	lalen = sizeof la;
	ralen = sizeof ra;
	conn_uap = conn->uap;
	conn_func = conn->func;
	id.opaque = conn;
#ifdef SO_ERROR
	optlen = sizeof socket_errno;
	if (fd < 0 &&
	    getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, (char *)&socket_errno,
		       &optlen) < 0)
		socket_errno = errno;
	else
		errno = socket_errno;
#endif
	if (evCancelConn(opaqueCtx, id) < 0 ||
	    socket_errno ||
#ifdef NETREAD_BROKEN
	    0 ||
#else
	    read(fd, buf, 0) < 0 ||
#endif
	    getsockname(fd, &la, &lalen) < 0 ||
	    getpeername(fd, &ra, &ralen) < 0) {
		int save = errno;

		(void) close(fd);	/* XXX closing caller's fd */
		errno = save;
		fd = -1;
	}
	(*conn_func)(opaqueCtx, conn_uap, fd, &la, lalen, &ra, ralen);
}

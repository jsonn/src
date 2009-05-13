/*	$NetBSD: local.c,v 1.1.1.1.2.2 2009/05/13 18:52:40 jym Exp $	*/

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <linux/connector.h>
#include <linux/netlink.h>

#include "linux/dm-clog-tfr.h"
#include "functions.h"
#include "cluster.h"
#include "common.h"
#include "logging.h"
#include "link_mon.h"
#include "local.h"

static int cn_fd;  /* Connector (netlink) socket fd */
static char recv_buf[2048];


/* FIXME: merge this function with kernel_send_helper */
static int kernel_ack(uint32_t seq, int error)
{
	int r;
	unsigned char buf[sizeof(struct nlmsghdr) + sizeof(struct cn_msg)];
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct cn_msg *msg = NLMSG_DATA(nlh);

	if (error < 0) {
		LOG_ERROR("Programmer error: error codes must be positive");
		return -EINVAL;
	}

	memset(buf, 0, sizeof(buf));

	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct cn_msg));
	nlh->nlmsg_flags = 0;

	msg->len = 0;
	msg->id.idx = 0x4;
	msg->id.val = 0x1;
	msg->seq = seq;
	msg->ack = error;

	r = send(cn_fd, nlh, NLMSG_LENGTH(sizeof(struct cn_msg)), 0);
	/* FIXME: do better error processing */
	if (r <= 0)
		return -EBADE;

	return 0;
}


/*
 * kernel_recv
 * @tfr: the newly allocated request from kernel
 *
 * Read requests from the kernel and allocate space for the new request.
 * If there is no request from the kernel, *tfr is NULL.
 *
 * This function is not thread safe due to returned stack pointer.  In fact,
 * the returned pointer must not be in-use when this function is called again.
 *
 * Returns: 0 on success, -EXXX on error
 */
static int kernel_recv(struct clog_tfr **tfr)
{
	int r = 0;
	int len;
	struct cn_msg *msg;

	*tfr = NULL;
	memset(recv_buf, 0, sizeof(recv_buf));

	len = recv(cn_fd, recv_buf, sizeof(recv_buf), 0);
	if (len < 0) {
		LOG_ERROR("Failed to recv message from kernel");
		r = -errno;
		goto fail;
	}

	switch (((struct nlmsghdr *)recv_buf)->nlmsg_type) {
	case NLMSG_ERROR:
		LOG_ERROR("Unable to recv message from kernel: NLMSG_ERROR");
		r = -EBADE;
		goto fail;
	case NLMSG_DONE:
		msg = (struct cn_msg *)NLMSG_DATA((struct nlmsghdr *)recv_buf);
		len -= sizeof(struct nlmsghdr);

		if (len < sizeof(struct cn_msg)) {
			LOG_ERROR("Incomplete request from kernel received");
			r = -EBADE;
			goto fail;
		}

		if (msg->len > DM_CLOG_TFR_SIZE) {
			LOG_ERROR("Not enough space to receive kernel request (%d/%d)",
				  msg->len, DM_CLOG_TFR_SIZE);
			r = -EBADE;
			goto fail;
		}

		if (!msg->len)
			LOG_ERROR("Zero length message received");

		len -= sizeof(struct cn_msg);

		if (len < msg->len)
			LOG_ERROR("len = %d, msg->len = %d", len, msg->len);

		msg->data[msg->len] = '\0'; /* Cleaner way to ensure this? */
		*tfr = (struct clog_tfr *)msg->data;

		if (!(*tfr)->request_type) {
			LOG_DBG("Bad transmission, requesting resend [%u]", msg->seq);
			r = -EAGAIN;

			if (kernel_ack(msg->seq, EAGAIN)) {
				LOG_ERROR("Failed to NACK kernel transmission [%u]",
					  msg->seq);
				r = -EBADE;
			}
		}
		break;
	default:
		LOG_ERROR("Unknown nlmsg_type");
		r = -EBADE;
	}

fail:
	if (r)
		*tfr = NULL;

	return (r == -EAGAIN) ? 0 : r;
}

static int kernel_send_helper(void *data, int out_size)
{
	int r;
	struct nlmsghdr *nlh;
	struct cn_msg *msg;
	unsigned char buf[2048];

	memset(buf, 0, sizeof(buf));

	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_seq = 0;  /* FIXME: Is this used? */
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_len = NLMSG_LENGTH(out_size + sizeof(struct cn_msg));
	nlh->nlmsg_flags = 0;

	msg = NLMSG_DATA(nlh);
	memcpy(msg->data, data, out_size);
	msg->len = out_size;
	msg->id.idx = 0x4;
	msg->id.val = 0x1;
	msg->seq = 0;

	r = send(cn_fd, nlh, NLMSG_LENGTH(out_size + sizeof(struct cn_msg)), 0);
	/* FIXME: do better error processing */
	if (r <= 0)
		return -EBADE;

	return 0;
}

/*
 * do_local_work
 *
 * Any processing errors are placed in the 'tfr'
 * structure to be reported back to the kernel.
 * It may be pointless for this function to
 * return an int.
 *
 * Returns: 0 on success, -EXXX on failure
 */
static int do_local_work(void *data)
{
	int r;
	struct clog_tfr *tfr = NULL;

	r = kernel_recv(&tfr);
	if (r)
		return r;

	if (!tfr)
		return 0;

	LOG_DBG("[%s]  Request from kernel received: [%s/%u]",
		SHORT_UUID(tfr->uuid), RQ_TYPE(tfr->request_type),
		tfr->seq);
	switch (tfr->request_type) {
	case DM_CLOG_CTR:
	case DM_CLOG_DTR:
	case DM_CLOG_IN_SYNC:
	case DM_CLOG_GET_SYNC_COUNT:
	case DM_CLOG_STATUS_INFO:
	case DM_CLOG_STATUS_TABLE:
	case DM_CLOG_PRESUSPEND:
		/* We do not specify ourselves as server here */
		r = do_request(tfr, 0);
		if (r)
			LOG_DBG("Returning failed request to kernel [%s]",
				RQ_TYPE(tfr->request_type));
		r = kernel_send(tfr);
		if (r)
			LOG_ERROR("Failed to respond to kernel [%s]",
				  RQ_TYPE(tfr->request_type));
			
		break;
	case DM_CLOG_RESUME:
		/*
		 * Resume is a special case that requires a local
		 * component to join the CPG, and a cluster component
		 * to handle the request.
		 */
		r = local_resume(tfr);
		if (r) {
			LOG_DBG("Returning failed request to kernel [%s]",
				RQ_TYPE(tfr->request_type));
			r = kernel_send(tfr);
			if (r)
				LOG_ERROR("Failed to respond to kernel [%s]",
					  RQ_TYPE(tfr->request_type));
			break;
		}
		/* ELSE, fall through */
	case DM_CLOG_IS_CLEAN:
	case DM_CLOG_FLUSH:
	case DM_CLOG_MARK_REGION:
	case DM_CLOG_GET_RESYNC_WORK:
	case DM_CLOG_SET_REGION_SYNC:
	case DM_CLOG_IS_REMOTE_RECOVERING:
	case DM_CLOG_POSTSUSPEND:
		r = cluster_send(tfr);
		if (r) {
			tfr->data_size = 0;
			tfr->error = r;
			kernel_send(tfr);
		}

		break;
	case DM_CLOG_CLEAR_REGION:
		r = kernel_ack(tfr->seq, 0);

		r = cluster_send(tfr);
		if (r) {
			/*
			 * FIXME: store error for delivery on flush
			 *        This would allow us to optimize MARK_REGION
			 *        too.
			 */
		}

		break;
	case DM_CLOG_GET_REGION_SIZE:
	default:
		LOG_ERROR("Invalid log request received, ignoring.");
		return 0;
	}

	if (r && !tfr->error)
		tfr->error = r;

	return r;
}

/*
 * kernel_send
 * @tfr: result to pass back to kernel
 *
 * This function returns the tfr structure
 * (containing the results) to the kernel.
 * It then frees the structure.
 *
 * WARNING: should the structure be freed if
 * there is an error?  I vote 'yes'.  If the
 * kernel doesn't get the response, it should
 * resend the request.
 *
 * Returns: 0 on success, -EXXX on failure
 */
int kernel_send(struct clog_tfr *tfr)
{
	int r;
	int size;

	if (!tfr)
		return -EINVAL;

	size = sizeof(struct clog_tfr) + tfr->data_size;

	if (!tfr->data_size && !tfr->error) {
		/* An ACK is all that is needed */

		/* FIXME: add ACK code */
	} else if (size > DM_CLOG_TFR_SIZE) {
		/*
		 * If we gotten here, we've already overrun
		 * our allotted space somewhere.
		 *
		 * We must do something, because the kernel
		 * is waiting for a response.
		 */
		LOG_ERROR("Not enough space to respond to server");
		tfr->error = -ENOSPC;
		size = sizeof(struct clog_tfr);
	}

	r = kernel_send_helper(tfr, size);
	if (r)
		LOG_ERROR("Failed to send msg to kernel.");

	return r;
}

/*
 * init_local
 *
 * Initialize kernel communication socket (netlink)
 *
 * Returns: 0 on success, values from common.h on failure
 */
int init_local(void)
{
	int r = 0;
	int opt;
	struct sockaddr_nl addr;

	cn_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (cn_fd < 0)
		return EXIT_KERNEL_TFR_SOCKET;

	/* memset to fix valgrind complaint */
	memset(&addr, 0, sizeof(struct sockaddr_nl));

	addr.nl_family = AF_NETLINK;
	addr.nl_groups = 0x4;
	addr.nl_pid = 0;

	r = bind(cn_fd, (struct sockaddr *) &addr, sizeof(addr));
	if (r < 0) {
		close(cn_fd);
		return EXIT_KERNEL_TFR_BIND;
	}

	opt = addr.nl_groups;
	r = setsockopt(cn_fd, 270, NETLINK_ADD_MEMBERSHIP, &opt, sizeof(opt));
	if (r) {
		close(cn_fd);
		return EXIT_KERNEL_TFR_SETSOCKOPT;
	}

	/*
	r = fcntl(cn_fd, F_SETFL, FNDELAY);
	*/

	links_register(cn_fd, "local", do_local_work, NULL);

	return 0;
}

/*
 * cleanup_local
 *
 * Clean up before exiting
 */
void cleanup_local(void)
{
	links_unregister(cn_fd);
	close(cn_fd);
}

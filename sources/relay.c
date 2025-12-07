/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>
#include <odyssey.h>

static inline od_frontend_status_t
od_relay_start(od_relay_mode_t mode, od_client_t *client, od_relay_t *relay)
{
	relay->mode = mode;
	relay->client = client;

	if (relay->iov == NULL) {
		relay->iov = machine_iov_create();
	}
	if (relay->iov == NULL) {
		return OD_EOOM;
	}

	machine_cond_t *client_io_cond = od_client_get_io_cond(client);

	machine_cond_propagate(relay->src->on_read, client_io_cond);
	machine_cond_propagate(relay->src->on_write, client_io_cond);

	int rc;
	rc = od_io_read_start(relay->src);
	if (rc == -1)
		return od_relay_get_read_error(relay);

	/*
	 * If there is no new data from client we must reset read condition
	 * to avoid attaching to a new server connection
	 */

	if (machine_cond_try(relay->src->on_read)) {
		rc = od_relay_read_pending_aware(relay);
		if (rc != OD_OK)
			return rc;

		int rssc =
			client->route->rule->reserve_session_server_connection;

		/* signal machine condition immediately if we are not requested for pending data wait */
		if (od_likely(!rssc || od_relay_data_pending(relay))) {
			/* Seems like some data arrived */
			machine_cond_signal(relay->src->on_read);
		}
	}

	return OD_OK;
}

od_frontend_status_t od_relay_start_client_to_server(od_client_t *client,
						     od_relay_t *relay)
{
	return od_relay_start(OD_RELAY_MODE_CLIENT_TO_SERVER, client, relay);
}

od_frontend_status_t od_relay_start_server_to_client(od_client_t *client,
						     od_relay_t *relay)
{
	return od_relay_start(OD_RELAY_MODE_SERVER_TO_CLIENT, client, relay);
}

void od_relay_update_stats(od_relay_t *relay, int size)
{
	od_stat_t *stats = &relay->client->route->stats;

	switch (relay->mode) {
	case OD_RELAY_MODE_CLIENT_TO_SERVER:
		od_stat_recv_client(stats, size);
		break;

	case OD_RELAY_MODE_SERVER_TO_CLIENT:
		od_stat_recv_server(stats, size);
		break;

	default:
		abort();
	}
}

od_frontend_status_t od_relay_handle_packet(od_relay_t *relay, char *msg,
					    int size)
{
	switch (relay->mode) {
	case OD_RELAY_MODE_CLIENT_TO_SERVER:
		return od_frontend_remote_client_handle_packet(relay, msg,
							       size);

	case OD_RELAY_MODE_SERVER_TO_CLIENT:
		return od_frontend_remote_server_handle_packet(relay, msg,
							       size);

	default:
		abort();
	}
}

bool od_relay_data_pending(od_relay_t *relay)
{
	return od_readahead_unread(&relay->src->readahead) > 0;
}

void od_relay_free(od_relay_t *relay)
{
	if (relay->packet_full) {
		machine_msg_free(relay->packet_full);
	}

	if (relay->iov) {
		machine_iov_free(relay->iov);
	}
}

void od_relay_detach(od_relay_t *relay)
{
	if (!relay->dst)
		return;

	/*
	 * at od_relay_start() server->io.on_read/on_write was
	 * propagated to client->io_cond
	 *
	 * when server connect is detached from client, it is necessary to disable
	 * propagation, otherwise there might be sigsegv, when endpoint status (tsa for ex.)
	 * is checked before attaching to next client, but on_read was propagated to freed client
	 */
	if (relay->mode == OD_RELAY_MODE_SERVER_TO_CLIENT) {
		machine_cond_propagate(relay->src->on_read, NULL);
		machine_cond_propagate(relay->src->on_write, NULL);
	}

	od_io_write_stop(relay->dst);
	relay->dst = NULL;
}

int od_relay_stop(od_relay_t *relay)
{
	od_relay_detach(relay);
	od_io_read_stop(relay->src);
	return 0;
}

od_frontend_status_t od_relay_on_packet_msg(od_relay_t *relay,
							  machine_msg_t *msg)
{
	int rc;
	od_frontend_status_t status;
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);

	status = od_relay_handle_packet(relay, data, size);

	switch (status) {
	case OD_OK:
	/* fallthrough */
	case OD_DETACH:
		rc = machine_iov_add(relay->iov, msg);
		if (rc == -1)
			return OD_EOOM;
		break;
	case OD_SKIP:
		status = OD_OK;
	/* fallthrough */
	case OD_REQ_SYNC:
	/* fallthrough */
	default:
		machine_msg_free(msg);
		break;
	}
	return status;
}

od_frontend_status_t
od_relay_process(od_relay_t *relay, int *progress, char *data, int size)
{
	int rc;

	*progress = 0;

	if (od_relay_at_packet_begin(relay)) {
		/* If we are parsing beginning of next package, there should be no delayed packet*/
		assert(relay->packet_full == NULL);
		assert(relay->packet_full_pos == 0);
		if (size < (int)sizeof(kiwi_header_t))
			return OD_UNDEF;

		uint32_t body;
		rc = kiwi_validate_header(data, sizeof(kiwi_header_t), &body);
		if (rc != 0)
			return OD_ESYNC_BROKEN;

		body -= sizeof(uint32_t);

		int packet_size = sizeof(kiwi_header_t) + body;
		if (size >= packet_size) {
			/* there are enough bytes to process full packet */
			machine_msg_t *msg = machine_msg_create(packet_size);
			if (msg == NULL) {
				return OD_EOOM;
			}

			memcpy(machine_msg_data(msg), data, packet_size);

			*progress = packet_size;

			return od_relay_on_packet_msg(relay, msg);
		}

		*progress = size;

		relay->packet_bytes_read_left = packet_size - size;

		relay->packet_full = machine_msg_create(packet_size);
		if (relay->packet_full == NULL)
			return OD_EOOM;
		char *dest;
		dest = machine_msg_data(relay->packet_full);
		memcpy(dest, data, size);
		relay->packet_full_pos = size;
		return OD_OK;
	}

	/*
	 * chunk
	 * package reading already started, lets advance in it
	*/
	int to_parse = relay->packet_bytes_read_left;
	if (to_parse > size)
		to_parse = size;
	*progress = to_parse;
	relay->packet_bytes_read_left -= to_parse;

	char *dest;
	dest = machine_msg_data(relay->packet_full);
	memcpy(dest + relay->packet_full_pos, data, to_parse);
	relay->packet_full_pos += to_parse;

	/* if need to read more bytes of packet - wait for it */
	if (relay->packet_bytes_read_left > 0) {
		return OD_OK;
	}

	/* reading of current packet finished - lets handle it */
	machine_msg_t *msg = relay->packet_full;
	relay->packet_full = NULL;
	relay->packet_full_pos = 0;

	return od_relay_on_packet_msg(relay, msg);
}

od_frontend_status_t od_relay_pipeline(od_relay_t *relay)
{
	int progress;
	od_frontend_status_t rc;
	od_readahead_t *rahead = &relay->src->readahead;

	while (od_readahead_unread(rahead) > 0) {
		struct iovec rvec = od_readahead_read_begin(rahead);
		rc = od_relay_process(relay, &progress, rvec.iov_base,
				      rvec.iov_len);
		od_readahead_read_commit(rahead, (size_t)progress);
		if (rc == OD_REQ_SYNC) {
			return OD_REQ_SYNC;
		}
		if (rc != OD_OK) {
			if (rc == OD_UNDEF) {
				/*
				 * case when we can't process bytes, but want to retry later
				 * ex: at packet begin we read less bytes than header size
				 */
				return OD_OK;
			}

			return rc;
		}
	}
	return OD_OK;
}

od_frontend_status_t od_relay_read(od_relay_t *relay)
{
	od_readahead_t *rahead = &relay->src->readahead;
	struct iovec wvec = od_readahead_write_begin(rahead);
	if (wvec.iov_len == 0) {
		if (machine_read_pending(relay->src->io)) {
			/*
			 * This is situation, when we can read some bytes
			 * but there is no place in readahead for it.
			 * Therefore, this should be retry later, when readahead will
			 * have free space to place the bytes.
			 *
			 * Should signal to retry read later
			*/
			machine_cond_signal(relay->src->on_read);
			return OD_READAHEAD_IS_FULL;
		}
		return OD_OK;
	}

	int rc;
	rc = machine_read_raw(relay->src->io, wvec.iov_base, wvec.iov_len);
	if (rc <= 0) {
		/* retry */
		int errno_ = machine_errno();
		if (errno_ == EAGAIN || errno_ == EWOULDBLOCK ||
		    errno_ == EINTR)
			return OD_OK;
		/* error or eof */
		return od_relay_get_read_error(relay);
	}

	od_readahead_write_commit(rahead, (size_t)rc);

	/* update recv stats */
	od_relay_update_stats(relay, rc /* size */);

	return OD_OK;
}

od_frontend_status_t od_relay_write(od_relay_t *relay)
{
	assert(relay->dst);

	if (!machine_iov_pending(relay->iov))
		return OD_OK;

	int rc;
	rc = machine_writev_raw(relay->dst->io, relay->iov);
	if (rc < 0) {
		/* retry or error */
		int errno_ = machine_errno();
		if (errno_ == EAGAIN || errno_ == EWOULDBLOCK ||
		    errno_ == EINTR) {
			machine_sleep(1);
			return OD_OK;
		}
		return od_relay_get_write_error(relay);
	}

	return OD_OK;
}

od_frontend_status_t od_relay_step(od_relay_t *relay,
						 bool await_read)
{
	/* on read event */
	od_frontend_status_t retstatus;
	retstatus = OD_OK;
	int rc;
	int should_try_read;
	int pending;
	should_try_read = await_read ? (machine_cond_wait(relay->src->on_read,
							  UINT32_MAX) == 0) :
				       machine_cond_try(relay->src->on_read);

	pending = od_relay_data_pending(relay);
	if (should_try_read || pending) {
		rc = od_relay_read_pending_aware(relay);
		if (rc != OD_OK)
			return rc;

		/*
		 * TODO: do check first byte of next package, and
		 * if KIWI_FE_QUERY, try to parse attach-time hint
		 */
		if (relay->dst == NULL) {
			return OD_ATTACH;
		}
	}

	rc = od_relay_pipeline(relay);

	if (rc == OD_REQ_SYNC) {
		retstatus = OD_REQ_SYNC;
	} else if (rc != OD_OK)
		return rc;

	if (should_try_read || pending) {
		if (machine_iov_pending(relay->iov)) {
			/* try to optimize write path and handle it right-away */
			machine_cond_signal(relay->dst->on_write);
		}
	}

	if (relay->dst == NULL)
		return retstatus;

	/* on write event */
	if (machine_cond_try(relay->dst->on_write)) {
		rc = od_relay_write(relay);
		if (rc != OD_OK)
			return rc;

		if (!machine_iov_pending(relay->iov)) {
			rc = od_io_write_stop(relay->dst);
			if (rc == -1)
				return od_relay_get_write_error(relay);

			rc = od_io_read_start(relay->src);
			if (rc == -1)
				return od_relay_get_read_error(relay);
		} else {
			rc = od_io_write_start(relay->dst);
			if (rc == -1)
				return od_relay_get_write_error(relay);
		}
	}

	return retstatus;
}

od_frontend_status_t od_relay_flush(od_relay_t *relay)
{
	if (relay->dst == NULL)
		return OD_OK;

	if (!machine_iov_pending(relay->iov))
		return OD_OK;

	int rc;
	rc = od_relay_write(relay);
	if (rc != OD_OK)
		return rc;

	if (!machine_iov_pending(relay->iov))
		return OD_OK;

	rc = od_io_write_start(relay->dst);
	if (rc == -1)
		return od_relay_get_write_error(relay);

	for (;;) {
		if (!machine_iov_pending(relay->iov))
			break;

		machine_cond_wait(relay->dst->on_write, UINT32_MAX);

		rc = od_relay_write(relay);
		if (rc != OD_OK) {
			od_io_write_stop(relay->dst);
			return rc;
		}
	}

	rc = od_io_write_stop(relay->dst);
	if (rc == -1)
		return od_relay_get_write_error(relay);

	return OD_OK;
}

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>
#include "status.h"

typedef struct od_relay od_relay_t;

typedef enum {
	OD_RELAY_MODE_UNDEF,
	OD_RELAY_MODE_CLIENT_TO_SERVER, /* client -> server byte stream */
	OD_RELAY_MODE_SERVER_TO_CLIENT, /* server -> client byte stream */
} od_relay_mode_t;

struct od_relay {
	/* the amount of bytes needed to read current packet to the end */
	int packet_bytes_read_left;

	od_client_t *client;
	od_relay_mode_t mode;

	machine_msg_t *packet_full;
	int packet_full_pos;
	machine_iov_t *iov;
	od_io_t *src;
	od_io_t *dst;
};

static inline int od_relay_at_packet_begin(const od_relay_t *relay)
{
	return relay->packet_bytes_read_left == 0;
}

static inline od_frontend_status_t
od_relay_read_pending_aware(od_relay_t *relay);
static inline od_frontend_status_t od_relay_read(od_relay_t *relay);

static inline void od_relay_init(od_relay_t *relay, od_io_t *io)
{
	relay->packet_bytes_read_left = 0;
	relay->packet_full = NULL;
	relay->packet_full_pos = 0;
	relay->iov = NULL;
	relay->src = io;
	relay->dst = NULL;
	relay->client = NULL;
	relay->mode = OD_RELAY_MODE_UNDEF;
}

static inline od_frontend_status_t
od_relay_get_read_error(const od_relay_t *relay)
{
	switch (relay->mode) {
	/*
	 * for client -> server relay
	 * error of read means that read() from client failed
	 */
	case OD_RELAY_MODE_CLIENT_TO_SERVER:
		return OD_ECLIENT_READ;

	/*
	 * for server -> client relay
	 * error of read means that read() from server failed
	 */
	case OD_RELAY_MODE_SERVER_TO_CLIENT:
		return OD_ESERVER_READ;

	default:
		abort();
	}
}

static inline od_frontend_status_t
od_relay_get_write_error(const od_relay_t *relay)
{
	/* invert of read error value */
	switch (relay->mode) {
	/*
	 * for client -> server relay
	 * error of write means that write() on server failed
	 */
	case OD_RELAY_MODE_CLIENT_TO_SERVER:
		return OD_ESERVER_WRITE;

	/*
	 * for server -> client relay
	 * error of read means that write() on client failed
	 */
	case OD_RELAY_MODE_SERVER_TO_CLIENT:
		return OD_ECLIENT_WRITE;

	default:
		abort();
	}
}

static inline void od_relay_free(od_relay_t *relay)
{
	if (relay->packet_full) {
		machine_msg_free(relay->packet_full);
	}

	if (relay->iov) {
		machine_iov_free(relay->iov);
	}
}

static inline bool od_relay_data_pending(od_relay_t *relay)
{
	char *current = od_readahead_pos_read(&relay->src->readahead);
	char *end = od_readahead_pos(&relay->src->readahead);
	return current < end;
}

od_frontend_status_t od_relay_start_client_to_server(od_client_t *client,
						     od_relay_t *relay);

od_frontend_status_t od_relay_start_server_to_client(od_client_t *client,
						     od_relay_t *relay);

static inline void od_relay_attach(od_relay_t *relay, od_io_t *dst)
{
	assert(relay->dst == NULL);
	relay->dst = dst;
}

static inline void od_relay_detach(od_relay_t *relay)
{
	if (!relay->dst)
		return;
	od_io_write_stop(relay->dst);
	relay->dst = NULL;
}

static inline int od_relay_stop(od_relay_t *relay)
{
	od_relay_detach(relay);
	od_io_read_stop(relay->src);
	return 0;
}

od_frontend_status_t od_relay_handle_packet(od_relay_t *relay, char *msg,
					    int size);

static inline od_frontend_status_t od_relay_on_packet_msg(od_relay_t *relay,
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

static inline od_frontend_status_t od_relay_on_packet(od_relay_t *relay,
						      char *data, int size)
{
	int rc;
	od_frontend_status_t status;
	status = od_relay_handle_packet(relay, data, size);

	switch (status) {
	case OD_OK:
		/* fallthrough */
	case OD_DETACH:
		rc = machine_iov_add_pointer(relay->iov, data, size);

		od_dbg_printf_on_dvl_lvl(1, "relay %p advance msg %c\n", relay,
					 *data);
		if (rc == -1)
			return OD_EOOM;
		break;
	case OD_REQ_SYNC:
		/* fallthrough */
		break;
	case OD_SKIP:
		status = OD_OK;
		break;
	default:
		break;
	}

	return status;
}

__attribute__((hot)) static inline od_frontend_status_t
od_relay_process(od_relay_t *relay, int *progress, char *data, int size)
{
	int rc;

	*progress = 0;

	if (od_relay_at_packet_begin(relay)) {
		/* If we are parsing beginning of next package, there should be no delayed packet*/
		assert(relay->packet_full == NULL);
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
			*progress = packet_size;
			return od_relay_on_packet(relay, data, packet_size);
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

static inline od_frontend_status_t od_relay_pipeline(od_relay_t *relay)
{
	char *current = od_readahead_pos_read(&relay->src->readahead);
	char *end = od_readahead_pos(&relay->src->readahead);
	while (current < end) {
		int progress;
		od_frontend_status_t rc;
		rc = od_relay_process(relay, &progress, current, end - current);
		current += progress;
		od_readahead_pos_read_advance(&relay->src->readahead, progress);
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

/*
 * This is just like od_relay_read, but we must signal on read cond, when
 * there are pending bytes, otherwise it will be lost
 * (in case for tls cached bytes for example)
*/
static inline od_frontend_status_t
od_relay_read_pending_aware(od_relay_t *relay)
{
	od_frontend_status_t rc = od_relay_read(relay);
	if (rc == OD_READAHEAD_IS_FULL) {
		return OD_OK;
	}

	return rc;
}

void od_relay_update_stats(od_relay_t *relay, int size);

/*
 * This can lead to lost of relay->src->on_read in case of full readahead
 * and some pending bytes available.
 * Consider using od_relay_read_pending_aware instead
 */
static inline od_frontend_status_t od_relay_read(od_relay_t *relay)
{
	int to_read;
	to_read = od_readahead_left(&relay->src->readahead);
	if (to_read == 0) {
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

	char *pos;
	pos = od_readahead_pos(&relay->src->readahead);

	int rc;
	rc = machine_read_raw(relay->src->io, pos, to_read);
	if (rc <= 0) {
		/* retry */
		int errno_ = machine_errno();
		if (errno_ == EAGAIN || errno_ == EWOULDBLOCK ||
		    errno_ == EINTR)
			return OD_OK;
		/* error or eof */
		return od_relay_get_read_error(relay);
	}

	od_readahead_pos_advance(&relay->src->readahead, rc);

	/* update recv stats */
	od_relay_update_stats(relay, rc /* size */);

	return OD_OK;
}

static inline od_frontend_status_t od_relay_write(od_relay_t *relay)
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

static inline od_frontend_status_t od_relay_step(od_relay_t *relay,
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
		} else {
			/*
			 * all messages in iov are written (no pending), so there are no pointers
			 * in iov that holds any address in the readahead
			 *
			 * and now we can read in readahead at the beggining again
			 */
			od_readahead_reuse(&relay->src->readahead);
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

			/*
			 * all messages in iov are written (no pending), so there are no pointers
			 * in iov that holds any address in the readahead
			 *
			 * and now we can read in readahead at the beggining again
			 */
			od_readahead_reuse(&relay->src->readahead);

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

static inline od_frontend_status_t od_relay_flush(od_relay_t *relay)
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

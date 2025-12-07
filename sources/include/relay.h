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

void od_relay_free(od_relay_t *relay);

bool od_relay_data_pending(od_relay_t *relay);

od_frontend_status_t od_relay_start_client_to_server(od_client_t *client,
						     od_relay_t *relay);

od_frontend_status_t od_relay_start_server_to_client(od_client_t *client,
						     od_relay_t *relay);

static inline void od_relay_attach(od_relay_t *relay, od_io_t *dst)
{
	assert(relay->dst == NULL);
	relay->dst = dst;
}

void od_relay_detach(od_relay_t *relay);

int od_relay_stop(od_relay_t *relay);

od_frontend_status_t od_relay_handle_packet(od_relay_t *relay, char *msg,
					    int size);

od_frontend_status_t od_relay_on_packet_msg(od_relay_t *relay,
					    machine_msg_t *msg);

od_frontend_status_t od_relay_process(od_relay_t *relay, int *progress,
				      char *data, int size);

od_frontend_status_t od_relay_pipeline(od_relay_t *relay);

/*
 * This can lead to lost of relay->src->on_read in case of full readahead
 * and some pending bytes available.
 * Consider using od_relay_read_pending_aware instead
 */
od_frontend_status_t od_relay_read(od_relay_t *relay);

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

od_frontend_status_t od_relay_write(od_relay_t *relay);

od_frontend_status_t od_relay_step(od_relay_t *relay, bool await_read);

od_frontend_status_t od_relay_flush(od_relay_t *relay);

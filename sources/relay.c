/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

static inline od_frontend_status_t
od_relay_start(od_relay_mode_t mode, od_client_t *client, od_relay_t *relay,
	       od_relay_on_packet_t on_packet)
{
	relay->mode = mode;
	relay->on_packet = on_packet;
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

od_frontend_status_t
od_relay_start_client_to_server(od_client_t *client, od_relay_t *relay,
				od_relay_on_packet_t on_packet)
{
	return od_relay_start(OD_RELAY_MODE_CLIENT_TO_SERVER, client, relay,
			      on_packet);
}

od_frontend_status_t
od_relay_start_server_to_client(od_client_t *client, od_relay_t *relay,
				od_relay_on_packet_t on_packet)
{
	return od_relay_start(OD_RELAY_MODE_SERVER_TO_CLIENT, client, relay,
			      on_packet);
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

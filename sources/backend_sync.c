/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

int od_backend_request_sync_point(od_server_t *server)
{
	od_instance_t *instance = server->global->instance;
	int rc;

	machine_msg_t *msg;
	msg = kiwi_fe_write_sync(NULL);
	if (msg == NULL) {
		return -1;
	}
	rc = od_write(&server->io, msg);
	if (rc == -1) {
		od_error(&instance->logger, "sync-point", server->client,
			 server, "write error: %s", od_io_error(&server->io));
		return NOT_OK_RESPONSE;
	}

	/* update server sync state */
	od_server_sync_request(server, 1);

	return od_backend_ready_wait(server, "sync-point", 1 /*count*/,
				     UINT32_MAX /* timeout */,
				     0 /*ignore error?*/);
}
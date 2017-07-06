
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/syslog.h"
#include "sources/log.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/lex.h"
#include "sources/config.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/instance.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/relay.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/console.h"

typedef struct
{
	od_consolestatus_t status;
	od_client_t *client;
	char *request;
	machine_queue_t *response;
} od_msgconsole_t;

static void
od_console(void *arg)
{
	od_console_t *console = arg;
	od_instance_t *instance = console->system->instance;
	(void)instance;

	for (;;) {
		machine_msg_t *msg;
		msg = machine_queue_get(console->queue, UINT32_MAX);
		if (msg == NULL)
			break;
		od_msg_t msg_type;
		msg_type = machine_msg_get_type(msg);
		switch (msg_type) {
		case OD_MCONSOLE_REQUEST:
		{
			od_msgconsole_t *msg_console;
			msg_console = machine_msg_get_data(msg);

			od_client_t *client = msg_console->client;
			shapito_stream_t *stream = &client->stream;

			od_debug_client(&instance->log, &client->id, "console",
			                "%s", msg_console->request);

			shapito_stream_reset(stream);
			shapito_be_write_no_data(stream);
			shapito_be_write_ready(stream, 'I');

			msg_console->status = OD_COK;
			machine_queue_put(msg_console->response, msg);
			break;
		}
		default:
			assert(0);
			break;
		}
	}
}

int od_console_init(od_console_t *console, od_system_t *system)
{
	od_instance_t *instance = system->instance;
	console->system = system;
	console->queue = machine_queue_create();
	if (console->queue == NULL) {
		od_error(&instance->log, "console", "failed to create queue");
		return -1;
	}
	return 0;
}

int od_console_start(od_console_t *console)
{
	od_instance_t *instance = console->system->instance;
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_console, console);
	if (coroutine_id == -1) {
		od_error(&instance->log, "console", "failed to start console coroutine");
		return -1;
	}
	return 0;
}

static od_consolestatus_t
od_console_do(od_client_t *client, od_msg_t msg_type, char *request, int wait_for_response)
{
	od_console_t *console = client->system->console;

	/* send request to console */
	machine_msg_t *msg;
	msg = machine_msg_create(msg_type, sizeof(od_msgconsole_t));
	if (msg == NULL)
		return OD_CERROR;
	od_msgconsole_t *msg_console;
	msg_console = machine_msg_get_data(msg);
	msg_console->status = OD_CERROR;
	msg_console->client = client;
	msg_console->request = request;
	msg_console->response = NULL;

	/* create response queue */
	machine_queue_t *response;
	if (wait_for_response) {
		response = machine_queue_create();
		if (response == NULL) {
			machine_msg_free(msg);
			return OD_CERROR;
		}
		msg_console->response = response;
	}
	machine_queue_put(console->queue, msg);

	if (! wait_for_response)
		return OD_COK;

	/* wait for reply */
	msg = machine_queue_get(response, UINT32_MAX);
	if (msg == NULL) {
		/* todo:  */
		abort();
		machine_queue_free(response);
		return OD_CERROR;
	}
	msg_console = machine_msg_get_data(msg);
	od_consolestatus_t status;
	status = msg_console->status;
	machine_queue_free(response);
	machine_msg_free(msg);
	return status;
}

od_consolestatus_t
od_console_request(od_client_t *client, char *request)
{
	return od_console_do(client, OD_MCONSOLE_REQUEST, request, 1);
}

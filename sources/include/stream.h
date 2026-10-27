#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 * 
 * Streaming subsytem mo make some parts of stream more efficient
 * by gathering several io operations into one
 */

#include <kiwi/header.h>

#include <status.h>
#include <types.h>
#include <wait_event.h>

od_frontend_status_t od_stream_server_until_rfq(od_server_t *server,
						uint32_t timeout_ms,
						od_wait_event_t wait_event);

od_frontend_status_t od_stream_copy_to_server(od_client_t *client,
					      od_server_t *server,
					      machine_msg_t *additional,
					      uint32_t timeout_ms);

od_frontend_status_t
od_service_stream_server_until_rfq(od_server_t *server, int ignore_errors,
				   uint32_t timeout_ms,
				   od_wait_event_t wait_event);

od_frontend_status_t od_stream_server_sync(od_server_t *server,
					   uint32_t timeout_ms,
					   od_wait_event_t wait_event);

typedef od_frontend_status_t (*od_stream_on_responce_cb)(kiwi_be_type_t type,
							 void *arg);

od_frontend_status_t od_stream_server_exact_completes(
	od_server_t *server, int n, od_stream_on_responce_cb on_response,
	void *arg, uint32_t timeout_ms, od_wait_event_t wait_event);

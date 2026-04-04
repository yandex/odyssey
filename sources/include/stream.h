#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 * 
 * Streaming subsytem mo make some parts of stream more efficient
 * by gathering several io operations into one
 */

#include <status.h>
#include <types.h>

od_frontend_status_t od_stream_server_until_rfq(char *ctx, od_server_t *server,
						uint32_t timeout_ms);

od_frontend_status_t od_stream_copy_to_server(char *ctx, od_client_t *client,
					      od_server_t *server,
					      uint32_t timeout_ms);

od_frontend_status_t od_service_stream_server_until_rfq(char *ctx,
							od_server_t *server,
							int ignore_errors,
							uint32_t timeout_ms);

od_frontend_status_t od_stream_server_sync(char *ctx, od_server_t *server,
					   uint32_t timeout_ms);

od_frontend_status_t od_stream_server_exact_completes(char *ctx,
						      od_server_t *server,
						      int n,
						      uint32_t timeout_ms);

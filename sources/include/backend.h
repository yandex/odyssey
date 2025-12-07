#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi/kiwi.h>
#include <types.h>
#include <storage.h>
#include <tsa.h>
#include <tls_config.h>

int od_backend_connect(od_server_t *, char *, kiwi_params_t *, od_client_t *);

int od_backend_check_tsa(od_storage_endpoint_t *, char *, od_server_t *,
			 od_client_t *, od_target_session_attrs_t);

int od_backend_connect_cancel(od_server_t *, od_rule_storage_t *,
			      const od_address_t *, kiwi_key_t *);

/* need perform startup after this */
int od_backend_connect_to(od_server_t *, char *, const od_address_t *,
			  od_tls_opts_t *);

int od_backend_startup(od_server_t *server, kiwi_params_t *route_params,
		       od_client_t *client);

/*
 * startup preallocated connection
 * for connections that was created for min_pool_size
 */
int od_backend_startup_preallocated(od_server_t *server,
				    kiwi_params_t *route_params,
				    od_client_t *client);

void od_backend_close_connection(od_server_t *);
void od_backend_close(od_server_t *);
void od_backend_error(od_server_t *, char *, char *, uint32_t);

int od_backend_update_parameter(od_server_t *, char *, char *, uint32_t, int);
int od_backend_ready(od_server_t *, char *, uint32_t);
int od_backend_ready_wait(od_server_t *, char *, uint32_t, uint32_t);

od_retcode_t od_backend_query_send(od_server_t *server, char *context,
				   char *query, char *param, int len);
od_retcode_t od_backend_query(od_server_t *, char *, char *, char *, int,
			      uint32_t, uint32_t);

int od_backend_not_connected(od_server_t *);

int od_backend_need_startup(od_server_t *);

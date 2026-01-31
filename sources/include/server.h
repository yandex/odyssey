#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdatomic.h>

#include <types.h>
#include <io.h>
#include <relay.h>
#include <id.h>
#include <stat.h>
#include <hashmap.h>
#include <od_memory.h>
#include <build.h>
#include <scram.h>

typedef enum {
	OD_SERVER_UNDEF,
	OD_SERVER_IDLE,
	OD_SERVER_ACTIVE,
} od_server_state_t;

struct od_server {
	/*
	 * reference counter for backend connection
	 *
	 * we consider the following as reference:
	 * - functions od_server_create and od_server_free
	 *   (smth like "the pool holds the ref")
	 * - client connection that use the server
	 *   (od_server_attach_client and od_server_dettach_client)
	 * - each currently running cancel on the server
	 *   (od_server_cancel_begin and od_server_cancel_end)
	 * 
	 * every server that have ref counter > 1
	 * should have ACTIVE state,
	 * to prevent sending cancel to wrong client
	 * 
	 * not for directly usage
	 */
	atomic_int_fast64_t refs;
	od_server_state_t state;

	od_scram_state_t scram_state;
	od_id_t id;
	machine_tls_t *tls;
	od_io_t io;
	od_relay_t relay;
	int is_transaction;
	/* Copy stmt state */
	uint64_t done_fail_response_received;
	uint64_t in_out_response_received;
	/**/
	int deploy_sync;
	od_stat_state_t stats_state;

	od_multi_pool_element_t *pool_element;

	uint64_t sync_request;
	uint64_t sync_reply;

	/* to swallow some internal msgs */
	machine_msg_t *parse_msg;
	int idle_time;

	kiwi_key_t key;
	kiwi_key_t key_client;
	kiwi_vars_t vars;

	machine_msg_t *error_connect;
	/* do not set this field directly, use od_server_attach_client */
	od_client_t *client;
	od_route_t *route;

	/* allocated prepared statements ids */
	od_hashmap_t *prep_stmts;
	int sync_point;
	machine_msg_t *sync_point_deploy_msg;

	int bind_failed;

	od_global_t *global;
	int offline;
	uint64_t init_time_us;
	bool synced_settings;

	od_list_t link;

	int need_startup;
};

static const size_t OD_SERVER_DEFAULT_HASHMAP_SZ = 420;

static inline void od_server_init(od_server_t *server, int reserve_prep_stmts)
{
	memset(server, 0, sizeof(od_server_t));
	server->state = OD_SERVER_UNDEF;
	atomic_init(&server->refs, 1);
	server->route = NULL;
	server->client = NULL;
	server->global = NULL;
	server->tls = NULL;
	server->idle_time = 0;
	server->is_transaction = 0;
	server->done_fail_response_received = 0;
	server->in_out_response_received = 0;
	server->deploy_sync = 0;
	server->sync_request = 0;
	server->sync_reply = 0;
	server->sync_point = 0;
	server->sync_point_deploy_msg = NULL;
	server->parse_msg = NULL;
	server->init_time_us = machine_time_us();
	server->error_connect = NULL;
	server->offline = 0;
	server->synced_settings = false;
	server->pool_element = NULL;
	server->bind_failed = 0;
	server->need_startup = 1;
	od_stat_state_init(&server->stats_state);

	od_scram_state_init(&server->scram_state);

	kiwi_key_init(&server->key);
	kiwi_key_init(&server->key_client);
	kiwi_vars_init(&server->vars);

	od_io_init(&server->io);
	od_relay_init(&server->relay, &server->io);
	od_list_init(&server->link);
	memset(&server->id, 0, sizeof(server->id));

	if (reserve_prep_stmts) {
		server->prep_stmts =
			od_hashmap_create(OD_SERVER_DEFAULT_HASHMAP_SZ);
	} else {
		server->prep_stmts = NULL;
	}
}

void od_server_attach_client(od_server_t *server, od_client_t *client);
void od_server_detach_client(od_server_t *server);

static inline od_server_t *od_server_allocate(int reserve_prep_stmts)
{
	od_server_t *server = od_malloc(sizeof(od_server_t));
	if (server == NULL) {
		return NULL;
	}
	od_server_init(server, reserve_prep_stmts);
	return server;
}

void od_server_free(od_server_t *server);

static inline void od_server_sync_request(od_server_t *server, uint64_t count)
{
	server->sync_request += count;
}

static inline void od_server_sync_reply(od_server_t *server)
{
	server->sync_reply++;
}

static inline int od_server_in_deploy(od_server_t *server)
{
	return server->deploy_sync > 0;
}

static inline int od_server_in_sync_point(od_server_t *server)
{
	return server->sync_point > 0;
}

static inline int od_server_synchronized(od_server_t *server)
{
	assert(server->sync_request >= server->sync_reply);
	return server->sync_request == server->sync_reply;
}

static inline int od_server_grac_shutdown(od_server_t *server)
{
	server->offline = 1;
	return 0;
}

od_server_pool_t *od_server_pool(od_server_t *server);
const od_address_t *od_server_pool_address(od_server_t *server);
void od_server_set_pool_state(od_server_t *server, od_server_state_t state);
void od_server_cancel_begin(od_server_t *server);
void od_server_cancel_end(od_server_t *server);

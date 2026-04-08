#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdatomic.h>

#include <machinarium/ds/hm.h>

#include <types.h>
#include <io.h>
#include <id.h>
#include <stat.h>
#include <od_memory.h>
#include <build.h>
#include <pstmt.h>
#include <list.h>
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
	int is_transaction;
	int is_error_tx;
	/* Copy stmt state */
	int copy_mode;
	/**/
	int deploy_sync;
	od_stat_state_t stats_state;

	od_multi_pool_element_t *pool_element;

	uint64_t sync_request;
	uint64_t sync_reply;
	int msg_broken;

	int idle_time;

	kiwi_key_t key;
	kiwi_key_t key_client;
	kiwi_vars_t vars;

	machine_msg_t *error_connect;
	/* do not set this field directly, use od_server_attach_client */
	od_client_t *client;
	int client_pinned;
	od_route_t *route;

	od_global_t *global;
	int offline;
	uint64_t init_time_us;
	bool synced_settings;

	od_list_t link;

	/* xproto state fields */
	int xproto_mode;
	int xproto_err;
	int cached_plan_broken;
	mm_hashmap_t *prep_stmts;

	int need_startup;
};

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
	server->is_error_tx = 0;
	server->copy_mode = 0;
	server->deploy_sync = 0;
	server->sync_request = 0;
	server->sync_reply = 0;
	server->init_time_us = machine_time_us();
	server->error_connect = NULL;
	server->offline = 0;
	server->synced_settings = false;
	server->pool_element = NULL;
	server->need_startup = 1;
	server->client_pinned = 0;
	server->msg_broken = 0;
	od_stat_state_init(&server->stats_state);

	od_scram_state_init(&server->scram_state);

	kiwi_key_init(&server->key);
	kiwi_key_init(&server->key_client);
	kiwi_vars_init(&server->vars);

	od_io_init(&server->io);
	od_list_init(&server->link);
	memset(&server->id, 0, sizeof(server->id));

	server->xproto_mode = 0;
	server->xproto_err = 0;
	server->cached_plan_broken = 0;
	if (reserve_prep_stmts) {
		server->prep_stmts = od_server_pstmt_hashmap_create();
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

static inline int od_server_synchronized(od_server_t *server)
{
	assert(server->sync_request >= server->sync_reply);
	return server->sync_request == server->sync_reply;
}

od_server_pool_t *od_server_pool(od_server_t *server);
const od_address_t *od_server_pool_address(od_server_t *server);
void od_server_set_pool_state(od_server_t *server, od_server_state_t state);
void od_server_cancel_begin(od_server_t *server);
void od_server_cancel_end(od_server_t *server);

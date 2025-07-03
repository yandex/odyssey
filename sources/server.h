#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef enum {
	OD_SERVER_UNDEF,
	OD_SERVER_IDLE,
	OD_SERVER_ACTIVE,
} od_server_state_t;

struct od_server {
	od_server_state_t state;
#ifdef POSTGRESQL_FOUND
	od_scram_state_t scram_state;
#endif
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
};

static const size_t OD_SERVER_DEFAULT_HASHMAP_SZ = 420;

static inline void od_server_init(od_server_t *server, int reserve_prep_stmts)
{
	memset(server, 0, sizeof(od_server_t));
	server->state = OD_SERVER_UNDEF;
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
	od_stat_state_init(&server->stats_state);

#ifdef POSTGRESQL_FOUND
	od_scram_state_init(&server->scram_state);
#endif

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

static inline od_server_t *od_server_allocate(int reserve_prep_stmts)
{
	od_server_t *server = malloc(sizeof(od_server_t));
	if (server == NULL)
		return NULL;
	od_server_init(server, reserve_prep_stmts);
	return server;
}

static inline void od_server_free(od_server_t *server)
{
	od_relay_free(&server->relay);
	od_io_free(&server->io);
	if (server->prep_stmts) {
		od_hashmap_free(server->prep_stmts);
	}
	free(server);
}

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

static inline int od_server_reload(od_attribute_unused() od_server_t *server)
{
	// TODO: set offline to 1 if storage/auth rules changed
	return 0;
}

od_server_pool_t *od_server_pool(od_server_t *server);
const od_address_t *od_server_pool_address(od_server_t *server);
void od_server_set_pool_state(od_server_t *server, od_server_state_t state);

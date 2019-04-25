#ifndef ODYSSEY_SERVER_H
#define ODYSSEY_SERVER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_server od_server_t;

typedef enum
{
	OD_SERVER_UNDEF,
	OD_SERVER_IDLE,
	OD_SERVER_ACTIVE
} od_server_state_t;

struct od_server
{
	od_server_state_t  state;
	od_id_t            id;
	machine_tls_t     *tls;
	od_io_t            io;
	od_relay_t         relay;
	int                is_allocated;
	int                is_transaction;
	int                is_copy;
	int                deploy_sync;
	od_stat_state_t    stats_state;
	uint64_t           sync_request;
	uint64_t           sync_reply;
	int                idle_time;
	kiwi_key_t         key;
	kiwi_key_t         key_client;
	kiwi_vars_t        vars;
	machine_msg_t     *error_connect;
	void              *client;
	void              *route;
	od_global_t       *global;
	od_list_t          link;
};

static inline void
od_server_init(od_server_t *server)
{
	server->state          = OD_SERVER_UNDEF;
	od_stat_state_init(&server->stats_state);
	kiwi_key_init(&server->key);
	kiwi_key_init(&server->key_client);
	kiwi_vars_init(&server->vars);
	od_io_init(&server->io);
	od_relay_init(&server->relay, &server->io);
	od_list_init(&server->link);
	memset(&server->id, 0, sizeof(server->id));
}

static inline od_server_t*
od_server_allocate(mcxt_context_t mcxt)
{
	assert(mcxt != NULL);

	od_server_t *server = mcxt_alloc_mem(mcxt, sizeof(*server), true);
	if (server == NULL)
		return NULL;

	od_server_init(server);
	server->is_allocated = 1;
	mcxt_incr_refcount(server);
	return server;
}

static inline void
od_server_free(od_server_t *server)
{
	if (server->is_allocated)
	{
		mcxt_decr_refcount(server);
		mcxt_free(server);
	}
}

static inline void
od_server_sync_request(od_server_t *server, uint64_t count)
{
	server->sync_request += count;
}

static inline void
od_server_sync_reply(od_server_t *server)
{
	server->sync_reply++;;
}

static inline int
od_server_in_deploy(od_server_t *server)
{
	return server->deploy_sync > 0;
}

static inline int
od_server_synchronized(od_server_t *server)
{
	return server->sync_request == server->sync_reply;
}

#endif /* ODYSSEY_SERVER_H */

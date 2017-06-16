#ifndef OD_SERVER_H
#define OD_SERVER_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_server od_server_t;

typedef enum
{
	OD_SUNDEF,
	OD_SIDLE,
	OD_SACTIVE,
	OD_SEXPIRE
} od_serverstate_t;

struct od_server
{
	od_serverstate_t  state;
	uint64_t          id;
	so_stream_t       stream;
	machine_io_t      *io;
	machine_tls_t     *tls;
	int               is_allocated;
	int               is_transaction;
	int               is_copy;
	int64_t           count_request;
	int64_t           count_reply;
	int               idle_time;
	so_key_t          key;
	so_key_t          key_client;
	uint64_t          last_client_id;
	void             *route;
	od_system_t      *system;
	od_list_t         link;
};

static inline int
od_server_is_sync(od_server_t *server) {
	return server->count_request == server->count_reply;
}

static inline void
od_server_init(od_server_t *server)
{
	server->state          = OD_SUNDEF;
	server->id             = 0;
	server->route          = NULL;
	server->system         = NULL;
	server->io             = NULL;
	server->tls            = NULL;
	server->idle_time      = 0;
	server->is_allocated   = 0;
	server->is_transaction = 0;
	server->is_copy        = 0;
	server->last_client_id = UINT64_MAX;
	server->count_request  = 0;
	server->count_reply    = 0;
	so_keyinit(&server->key);
	so_keyinit(&server->key_client);
	so_stream_init(&server->stream);
	od_list_init(&server->link);
}

static inline od_server_t*
od_server_allocate(void)
{
	od_server_t *server = malloc(sizeof(*server));
	if (server == NULL)
		return NULL;
	od_server_init(server);
	server->is_allocated = 1;
	return server;
}

static inline void
od_server_free(od_server_t *server)
{
	so_stream_free(&server->stream);
	if (server->is_allocated)
		free(server);
}

#endif /* OD_SERVER_H */

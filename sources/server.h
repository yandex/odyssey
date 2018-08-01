#ifndef OD_SERVER_H
#define OD_SERVER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_serverstat od_serverstat_t;
typedef struct od_server     od_server_t;

typedef enum
{
	OD_SUNDEF,
	OD_SIDLE,
	OD_SACTIVE,
	OD_SEXPIRE
} od_serverstate_t;

struct od_serverstat
{
	od_atomic_u64_t count_request;
	od_atomic_u64_t count_reply;
	od_atomic_u64_t count_query;
	od_atomic_u64_t count_tx;
	od_atomic_u64_t query_time;
	uint64_t        query_time_start;
	od_atomic_u64_t tx_time;
	uint64_t        tx_time_start;
	od_atomic_u64_t recv_server;
	od_atomic_u64_t recv_client;
	uint64_t        count_error;
};

struct od_server
{
	od_serverstate_t      state;
	od_id_t               id;
	shapito_parameters_t  params;
	machine_io_t         *io;
	machine_tls_t        *tls;
	int                   is_allocated;
	int                   is_transaction;
	int                   is_copy;
	int                   deploy_sync;
	od_serverstat_t       stats;
	int                   idle_time;
	shapito_key_t         key;
	shapito_key_t         key_client;
	od_id_t               last_client_id;
	void                 *client;
	void                 *route;
	od_global_t          *global;
	od_list_t             link;
};

static inline void
od_server_init(od_server_t *server)
{
	server->state          = OD_SUNDEF;
	server->route          = NULL;
	server->client         = NULL;
	server->global         = NULL;
	server->io             = NULL;
	server->tls            = NULL;
	server->idle_time      = 0;
	server->is_allocated   = 0;
	server->is_transaction = 0;
	server->is_copy        = 0;
	server->deploy_sync    = 0;
	memset(&server->stats, 0, sizeof(server->stats));
	shapito_key_init(&server->key);
	shapito_key_init(&server->key_client);
	shapito_parameters_init(&server->params);
	od_list_init(&server->link);
	memset(&server->id, 0, sizeof(server->id));
	memset(&server->last_client_id, 0, sizeof(server->last_client_id));
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
	shapito_parameters_free(&server->params);
	if (server->is_allocated)
		free(server);
}

static inline int
od_server_sync_is(od_server_t *server)
{
	return server->stats.count_request == server->stats.count_reply;
}

static inline void
od_server_sync_request(od_server_t *server, uint64_t count)
{
	od_atomic_u64_add(&server->stats.count_request, count);
}

static inline void
od_server_sync_reply(od_server_t *server)
{
	od_atomic_u64_inc(&server->stats.count_reply);
}

static inline void
od_server_stat_query_start(od_server_t *server)
{
	if (! server->stats.query_time_start)
		server->stats.query_time_start = machine_time();

	if (! server->stats.tx_time_start)
		server->stats.tx_time_start = machine_time();
}

static inline void
od_server_stat_query_end(od_server_t *server, int64_t *query_time, int64_t *tx_time)
{
	int64_t diff;
	if (server->stats.query_time_start) {
		diff = machine_time() - server->stats.query_time_start;
		if (diff > 0) {
			*query_time = diff;
			od_atomic_u64_add(&server->stats.query_time, diff);
			od_atomic_u64_inc(&server->stats.count_query);
		}
		server->stats.query_time_start = 0;
	}

	if (server->is_transaction)
		return;

	if (server->stats.tx_time_start) {
		diff = machine_time() - server->stats.tx_time_start;
		if (diff > 0) {
			*tx_time = diff;
			od_atomic_u64_add(&server->stats.tx_time, diff);
			od_atomic_u64_inc(&server->stats.count_tx);
		}
		server->stats.tx_time_start = 0;
	}
}

static inline void
od_server_stat_recv_server(od_server_t *server, uint64_t bytes)
{
	od_atomic_u64_add(&server->stats.recv_server, bytes);
}

static inline void
od_server_stat_recv_client(od_server_t *server, uint64_t bytes)
{
	od_atomic_u64_add(&server->stats.recv_client, bytes);
}

static inline void
od_server_stat_error(od_server_t *server)
{
	server->stats.count_error++;
}

#endif /* OD_SERVER_H */

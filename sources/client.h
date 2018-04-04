#ifndef OD_CLIENT_H
#define OD_CLIENT_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_client od_client_t;

typedef enum
{
	OD_CUNDEF,
	OD_CPENDING,
	OD_CACTIVE,
	OD_CQUEUE
} od_clientstate_t;

struct od_client
{
	od_clientstate_t      state;
	od_id_t               id;
	uint64_t              coroutine_id;
	uint64_t              coroutine_attacher_id;
	machine_io_t         *io;
	machine_tls_t        *tls;
	od_configroute_t     *config;
	od_configlisten_t    *config_listen;
	uint64_t              time_accept;
	uint64_t              time_setup;
	shapito_be_startup_t  startup;
	shapito_parameters_t  params;
	shapito_key_t         key;
	shapito_stream_t     *stream;
	od_server_t          *server;
	void                 *route;
	od_global_t          *global;
	od_list_t             link_pool;
	od_list_t             link;
};

static inline void
od_client_init(od_client_t *client)
{
	client->state = OD_CUNDEF;
	client->coroutine_id = 0;
	client->coroutine_attacher_id = 0;
	client->io = NULL;
	client->tls = NULL;
	client->config = NULL;
	client->config_listen = NULL;
	client->server = NULL;
	client->route = NULL;
	client->global = NULL;
	client->time_accept = 0;
	client->time_setup = 0;
	client->stream = NULL;
	shapito_be_startup_init(&client->startup);
	shapito_parameters_init(&client->params);
	shapito_key_init(&client->key);
	od_list_init(&client->link_pool);
	od_list_init(&client->link);
}

static inline od_client_t*
od_client_allocate(void)
{
	od_client_t *client = malloc(sizeof(*client));
	if (client == NULL)
		return NULL;
	od_client_init(client);
	return client;
}

static inline void
od_client_free(od_client_t *client)
{
	assert(client->stream == NULL);
	shapito_be_startup_free(&client->startup);
	shapito_parameters_free(&client->params);
	free(client);
}

static inline shapito_stream_t*
od_client_stream_attach(od_client_t *client, shapito_cache_t *cache)
{
	assert(client->stream == NULL);
	client->stream = shapito_cache_pop(cache);
	return client->stream;
}

static inline void
od_client_stream_detach(od_client_t *client, shapito_cache_t *cache)
{
	assert(client->stream != NULL);
	shapito_cache_push(cache, client->stream);
	client->stream = NULL;
}

#endif /* OD_CLIENT_H */

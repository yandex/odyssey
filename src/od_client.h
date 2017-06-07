#ifndef OD_CLIENT_H
#define OD_CLIENT_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
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
	od_clientstate_t state;
	uint64_t         id;
	uint64_t         coroutine_id;
	machine_io_t     io;
	od_schemeuser_t *scheme;
	so_bestartup_t   startup;
	so_key_t         key;
	so_stream_t      stream;
	od_server_t     *server;
	void            *route;
	od_system_t     *system;
	od_list_t        link_pool;
	od_list_t        link;
};

static inline void
od_client_init(od_client_t *client)
{
	client->state = OD_CUNDEF;
	client->id = 0;
	client->coroutine_id = 0;
	client->io = NULL;
	client->scheme = NULL;
	client->server = NULL;
	client->route = NULL;
	client->system = NULL;
	so_bestartup_init(&client->startup);
	so_keyinit(&client->key);
	so_stream_init(&client->stream);
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
	so_bestartup_free(&client->startup);
	so_stream_free(&client->stream);
	free(client);
}

#endif /* OD_CLIENT_H */

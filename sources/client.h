#ifndef ODYSSEY_CLIENT_H
#define ODYSSEY_CLIENT_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_client_ctl od_client_ctl_t;
typedef struct od_client     od_client_t;

typedef enum
{
	OD_CLIENT_UNDEF,
	OD_CLIENT_PENDING,
	OD_CLIENT_ACTIVE,
	OD_CLIENT_QUEUE
} od_client_state_t;

typedef enum
{
	OD_CLIENT_OP_NONE,
	OD_CLIENT_OP_KILL
} od_clientop_t;

struct od_client_ctl
{
	od_clientop_t op;
};

struct od_client
{
	od_client_state_t   state;
	od_id_t             id;
	od_client_ctl_t     ctl;
	uint64_t            coroutine_id;
	uint64_t            coroutine_attacher_id;
	machine_io_t       *io;
	machine_io_t       *io_notify;
	machine_tls_t      *tls;
	od_config_route_t  *config;
	od_config_listen_t *config_listen;
	uint64_t            time_accept;
	uint64_t            time_setup;
	kiwi_be_startup_t   startup;
	kiwi_params_t       params;
	kiwi_key_t          key;
	od_server_t        *server;
	void               *route;
	od_global_t        *global;
	od_list_t           link_pool;
	od_list_t           link;
};

static inline void
od_client_init(od_client_t *client)
{
	client->state = OD_CLIENT_UNDEF;
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
	client->ctl.op = OD_CLIENT_OP_NONE;
	kiwi_be_startup_init(&client->startup);
	kiwi_params_init(&client->params);
	kiwi_key_init(&client->key);
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
	kiwi_be_startup_free(&client->startup);
	kiwi_params_free(&client->params);
	free(client);
}

static inline void
od_client_notify(od_client_t *client)
{
	machine_msg_t *msg;
	msg = machine_msg_create(sizeof(uint64_t));
	*(uint64_t*)machine_msg_get_data(msg) = 1;
	machine_write(client->io_notify, msg);
}

static inline void
od_client_notify_read(od_client_t *client)
{
	machine_msg_t *msg;
	msg = machine_read(client->io_notify, sizeof(uint64_t), UINT32_MAX);
	if (msg)
		machine_msg_free(msg);
}

#endif /* ODYSSEY_CLIENT_H */

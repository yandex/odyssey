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
	OD_CLIENT_OP_NONE = 0,
	OD_CLIENT_OP_KILL = 1
} od_clientop_t;

struct od_client_ctl
{
	od_atomic_u32_t op;
};

struct od_client
{
	od_client_state_t   state;
	od_id_t             id;
	od_client_ctl_t     ctl;
	uint64_t            coroutine_id;
	machine_io_t       *io;
	machine_io_t       *io_notify;
	machine_tls_t      *tls;
	od_packet_t         packet_reader;
	od_rule_t          *rule;
	od_config_listen_t *config_listen;
	uint64_t            time_accept;
	uint64_t            time_setup;
	kiwi_be_startup_t   startup;
	kiwi_vars_t         vars;
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
	client->state         = OD_CLIENT_UNDEF;
	client->coroutine_id  = 0;
	client->io            = NULL;
	client->tls           = NULL;
	client->rule          = NULL;
	client->config_listen = NULL;
	client->server        = NULL;
	client->route         = NULL;
	client->global        = NULL;
	client->time_accept   = 0;
	client->time_setup    = 0;
	client->ctl.op        = OD_CLIENT_OP_NONE;
	kiwi_be_startup_init(&client->startup);
	kiwi_vars_init(&client->vars);
	kiwi_key_init(&client->key);
	od_packet_init(&client->packet_reader);
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

static inline uint32_t
od_client_ctl_of(od_client_t *client)
{
	return od_atomic_u32_of(&client->ctl.op);
}

static inline void
od_client_ctl_set(od_client_t *client, uint32_t op)
{
	od_atomic_u32_or(&client->ctl.op, op);
}

static inline void
od_client_ctl_unset(od_client_t *client, uint32_t op)
{
	od_atomic_u32_xor(&client->ctl.op, op);
}

static inline void
od_client_kill(od_client_t *client)
{
	od_client_ctl_set(client, OD_CLIENT_OP_KILL);
	od_client_notify(client);
}

#endif /* ODYSSEY_CLIENT_H */

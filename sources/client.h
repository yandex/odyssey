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
	mcxt_context_t		mcxt;

	od_client_state_t   state;
	od_id_t             id;
	od_client_ctl_t     ctl;
	uint64_t            coroutine_id;
	machine_tls_t      *tls;
	od_io_t             io;
	machine_cond_t     *cond;
	od_relay_t          relay;
	machine_io_t       *notify_io;
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
	client->tls           = NULL;
	client->cond          = NULL;
	client->rule          = NULL;
	client->config_listen = NULL;
	client->server        = NULL;
	client->route         = NULL;
	client->global        = NULL;
	client->time_accept   = 0;
	client->time_setup    = 0;
	client->notify_io     = NULL;
	client->ctl.op        = OD_CLIENT_OP_NONE;
	kiwi_be_startup_init(&client->startup);
	kiwi_vars_init(&client->vars);
	kiwi_key_init(&client->key);
	od_io_init(&client->io);
	od_relay_init(&client->relay, &client->io);
	od_list_init(&client->link_pool);
	od_list_init(&client->link);
}

static inline od_client_t*
od_client_allocate(mcxt_context_t mcxt)
{
	mcxt_context_t	client_mcxt = mcxt_new(mcxt),
					old;

	old = mcxt_switch_to(client_mcxt);
	od_client_t *client = mcxt_alloc0(sizeof(*client));

	if (client == NULL)
		return NULL;

	od_client_init(client);
	client->mcxt = client_mcxt;
	mcxt_switch_to(old);

	return client;
}

static inline void
od_client_free(od_client_t *client)
{
	od_relay_free(&client->relay);
	od_io_free(&client->io);
	if (client->cond)
		machine_cond_free(client->cond);

	mcxt_delete(client->mcxt);
}

static inline void
od_client_notify_read(od_client_t *client)
{
	uint64_t value;
	machine_read_raw(client->notify_io, &value, sizeof(value));
}

static inline void
od_client_notify(od_client_t *client)
{
	uint64_t value = 1;
	machine_write_raw(client->notify_io, &value, sizeof(value));
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

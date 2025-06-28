#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef enum {
	OD_CLIENT_UNDEF,
	OD_CLIENT_PENDING,
	OD_CLIENT_ACTIVE,
	OD_CLIENT_QUEUE
} od_client_state_t;

#define OD_CLIENT_MAX_PEERLEN 128

struct od_client {
	od_client_state_t state;
	od_pool_client_type_t type;
	od_id_t id;
	uint64_t coroutine_id;
	machine_tls_t *tls;
	od_io_t io;
	machine_cond_t *io_cond;
	od_relay_t relay;
	od_rule_t *rule;
	od_config_listen_t *config_listen;

	uint64_t time_accept;
	uint64_t time_setup;
	uint64_t time_last_active;

	bool is_watchdog;

	kiwi_be_startup_t startup;
	kiwi_vars_t vars;
	kiwi_key_t key;

	od_server_t *server;
	od_route_t *route;
	char peer[OD_CLIENT_MAX_PEERLEN];

	/* desc preparet statements ids */
	od_hashmap_t *prep_stmt_ids;

	/* passwd from config rule */
	kiwi_password_t password;

	/* user - proveded passwd, fallback to use this when no other option is available*/
	kiwi_password_t received_password;
	od_global_t *global;
	machine_list_t link_pool;
	machine_list_t link;

	/* Used to kill client in kill_client or odyssey reload */
	od_atomic_u64_t killed;

	/* storage_user & storage_password provided by ldapsearch result */
#ifdef LDAP_FOUND
	char *ldap_storage_username;
	int ldap_storage_username_len;
	char *ldap_storage_password;
	int ldap_storage_password_len;
	char *ldap_auth_dn;
#endif

	/* external_id for logging additional ifno about client */
	char *external_id;
};

static const size_t OD_CLIENT_DEFAULT_HASHMAP_SZ = 420;

static inline od_retcode_t od_client_init_hm(od_client_t *client)
{
	client->prep_stmt_ids = od_hashmap_create(OD_CLIENT_DEFAULT_HASHMAP_SZ);
	if (client->prep_stmt_ids == NULL) {
		return NOT_OK_RESPONSE;
	}
	return OK_RESPONSE;
}

static inline void od_client_init(od_client_t *client)
{
	client->state = OD_CLIENT_UNDEF;
	client->type = OD_POOL_CLIENT_EXTERNAL;
	client->coroutine_id = 0;
	client->tls = NULL;
	client->io_cond = NULL;
	client->rule = NULL;
	client->config_listen = NULL;
	client->server = NULL;
	client->route = NULL;
	client->global = NULL;
	client->time_accept = 0;
	client->time_setup = 0;
#ifdef LDAP_FOUND
	client->ldap_storage_username = NULL;
	client->ldap_storage_username_len = 0;
	client->ldap_storage_password = NULL;
	client->ldap_storage_password_len = 0;
	client->ldap_auth_dn = NULL;
#endif
	client->external_id = NULL;

	kiwi_be_startup_init(&client->startup);
	kiwi_vars_init(&client->vars);
	kiwi_key_init(&client->key);

	od_io_init(&client->io);
	od_relay_init(&client->relay, &client->io);

	kiwi_password_init(&client->password);
	kiwi_password_init(&client->received_password);

	machine_list_init(&client->link_pool);
	machine_list_init(&client->link);

	client->prep_stmt_ids = NULL;

	od_atomic_u64_set(&client->killed, 0);
}

static inline od_client_t *od_client_allocate(void)
{
	od_client_t *client = malloc(sizeof(od_client_t));
	if (client == NULL)
		return NULL;
	od_client_init(client);
	return client;
}

static inline void od_client_free(od_client_t *client)
{
	od_relay_free(&client->relay);
	od_io_free(&client->io);
	if (client->io_cond)
		machine_cond_free(client->io_cond);
	/* clear password if saved any */
	kiwi_password_free(&client->password);
	kiwi_password_free(&client->received_password);
	if (client->prep_stmt_ids) {
		od_hashmap_free(client->prep_stmt_ids);
	}
	if (client->external_id) {
		free(client->external_id);
	}
	free(client);
}

static inline void od_client_kill(od_client_t *client)
{
	od_atomic_u64_set(&client->killed, 1UL);
}

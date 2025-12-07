#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <atomic.h>
#include <pool.h>
#include <io.h>
#include <types.h>
#include <id.h>
#include <relay.h>
#include <rules.h>
#include <list.h>
#include <hashmap.h>
#include <od_ldap.h>

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
	od_list_t link_pool;
	od_list_t link;

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

	/* external_id for logging additional info about client authneticated with external auth */
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

void od_client_init(od_client_t *client);

static inline od_client_t *od_client_allocate(void)
{
	od_client_t *client = od_malloc(sizeof(od_client_t));
	if (client == NULL)
		return NULL;
	od_client_init(client);
	return client;
}

void od_client_free(od_client_t *client);

/*
 * for service clients (auth, watchdog, etc) usage
 *
 * adds od_io_close which is performed in od_frontend_close and
 * must be performed for service clients too
 */
static inline void od_client_free_extended(od_client_t *client)
{
	od_io_close(&client->io);
	od_client_free(client);
}

static inline void od_client_kill(od_client_t *client)
{
	od_atomic_u64_set(&client->killed, 1UL);
}

machine_cond_t *od_client_get_io_cond(od_client_t *client);

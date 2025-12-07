/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>
#include <kiwi/kiwi.h>

#include <client.h>
#include <atomic.h>
#include <relay.h>
#include <list.h>

machine_cond_t *od_client_get_io_cond(od_client_t *client)
{
	return client->io_cond;
}

void od_client_init(od_client_t *client)
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

	od_list_init(&client->link_pool);
	od_list_init(&client->link);

	client->prep_stmt_ids = NULL;

	od_atomic_u64_set(&client->killed, 0);
}

void od_client_free(od_client_t *client)
{
#ifdef LDAP_FOUND
	od_free(client->ldap_auth_dn);
#endif

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
		od_free(client->external_id);
	}
	od_free(client);
}

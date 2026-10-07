/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <client.h>
#include <server.h>
#include <pstmt.h>
#include <multi_pool.h>

void od_server_free(od_server_t *server)
{
	od_io_close(&server->io);
	od_io_free(&server->io);

	if (server->endpoint != NULL) {
		od_rules_storage_unref(server->endpoint->storage);
		server->endpoint = NULL;
	}

	if (server->tls != NULL) {
		machine_tls_free(server->tls);
		server->tls = NULL;
	}
	if (server->prep_stmts) {
		od_server_pstmts_free(server);
	}
	od_scram_state_free(&server->scram_state);
	od_free(server);
}

void od_server_attach_client(od_server_t *server, od_client_t *client)
{
	od_assert(server->client == NULL);
	od_assert(client->server == NULL);
	od_assert(server->state != OD_SERVER_ACTIVE);

	server->client = client;
	client->server = server;
	server->key_client = client->key;
	server->idle_time = 0;
	od_server_set_pool_state(server, OD_SERVER_ACTIVE);
}

void od_server_detach_client(od_server_t *server)
{
	od_client_t *client = server->client;

	od_assert(client != NULL);
	od_assert(server->state == OD_SERVER_ACTIVE);
	od_assert(server == client->server);

	client->server = NULL;
	server->client = NULL;

	/*
	 * this can lead to errors in PG like:
	 * PID 0 in cancel request did not match any process
	 *
	 * we currently believe that this is not real problem
	 * because it is better to have extra message in PG log
	 * that does nothing than sending cancel to another
	 * than current client
	 */
	kiwi_key_init(&server->key_client);

	od_server_set_pool_state(server, OD_SERVER_IDLE);
}

od_server_pool_t *od_server_pool(od_server_t *server)
{
	return &server->pool_element->pool;
}

const od_address_t *od_server_pool_address(od_server_t *server)
{
	return &server->pool_element->key.address;
}

void od_server_set_pool_state(od_server_t *server, od_server_state_t state)
{
	od_server_pool_t *pool;
	pool = od_server_pool(server);

	od_pg_server_pool_set(pool, server, state);

	if (state == OD_SERVER_UNDEF) {
		server->pool_element = NULL;
		if (server->endpoint != NULL) {
			od_rules_storage_unref(server->endpoint->storage);
			server->endpoint = NULL;
		}
	}
}

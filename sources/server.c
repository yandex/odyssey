/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <client.h>
#include <server.h>
#include <multi_pool.h>

void od_server_attach_client(od_server_t *server, od_client_t *client)
{
	assert(server->client == NULL);
	assert(server->state != OD_SERVER_ACTIVE);

	server->client = client;
	client->server = server;
	server->key_client = client->key;
	server->idle_time = 0;
	od_server_set_pool_state(server, OD_SERVER_ACTIVE);
}

void od_server_detach_client(od_server_t *server)
{
	od_client_t *client = server->client;

	assert(client != NULL);
	assert(server->state == OD_SERVER_ACTIVE);
	assert(server == client->server);

	client->server = NULL;
	server->client = NULL;
	kiwi_key_init(&server->key_client);
	od_server_set_pool_state(server, OD_SERVER_IDLE);
}

od_server_pool_t *od_server_pool(od_server_t *server)
{
	return &server->pool_element->pool;
}

const od_address_t *od_server_pool_address(od_server_t *server)
{
	return &server->pool_element->address;
}

void od_server_set_pool_state(od_server_t *server, od_server_state_t state)
{
	od_server_pool_t *pool;
	pool = od_server_pool(server);

	od_pg_server_pool_set(pool, server, state);

	if (state == OD_SERVER_UNDEF) {
		server->pool_element = NULL;
	}
}

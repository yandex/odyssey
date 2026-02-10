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

static inline void od_server_free_now(od_server_t *server)
{
	od_relay_free(&server->relay);
	od_io_close(&server->io);
	od_io_free(&server->io);
	if (server->tls != NULL) {
		machine_tls_free(server->tls);
		server->tls = NULL;
	}
	if (server->prep_stmts) {
		od_hashmap_free(server->prep_stmts);
	}
	od_scram_state_free(&server->scram_state);
	od_free(server);
}

static inline int64_t od_server_ref(od_server_t *server)
{
	return atomic_fetch_add(&server->refs, 1);
}

static inline int64_t od_server_unref(od_server_t *server)
{
	int64_t prev = atomic_fetch_sub(&server->refs, 1);
	if (prev == 1) {
		od_server_free_now(server);
	}

	return prev;
}

void od_server_free(od_server_t *server)
{
	od_server_unref(server);
}

void od_server_cancel_begin(od_server_t *server)
{
	od_server_ref(server);
}

void od_server_cancel_end(od_server_t *server)
{
	int64_t prev_refs = od_server_unref(server);

	if (prev_refs == 2) {
		/*
		 * only cancels or "pool" holds the server
		 *
		 * if only cancels holds the server, it state
		 * and pool_element will be UNDEF and NULL respectively,
		 * (all cases of closing server means state=UNDEF)
		 * in that case nothing to be done - the server is closed
		 * and removed from pool by the router, and the next
		 * cancel end will free it
		 * 
		 * if the cancel and the pool holds the server, it can be
		 * return to IDLE state and reused by another client
		 * 
		 * the cases can de separated by server->state value
		 */

		if (server->state == OD_SERVER_ACTIVE) {
			od_server_set_pool_state(server, OD_SERVER_IDLE);
		}
	} else {
		/*
		 * if prev_refs > 2, then
		 *  nothing can be done - the client is not detached
		 *  or there is another cancels for this server
		 * 
		 * if prev_refs == 1, then
		 *  server was removed from pool
		 *  while the cancel was performed
		 *  ex: еhe connection failed and detached from client
		 * 
		 *  cancel was the only one who used the server, and now
		 *  this unref freed the server - nothing to be done now
		 */
	}
}

void od_server_attach_client(od_server_t *server, od_client_t *client)
{
	assert(server->client == NULL);
	assert(client->server == NULL);
	assert(server->state != OD_SERVER_ACTIVE);

	server->client = client;
	client->server = server;
	server->key_client = client->key;
	server->idle_time = 0;
	od_server_set_pool_state(server, OD_SERVER_ACTIVE);
	od_server_ref(server);
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

	if (od_server_unref(server) == 2) {
		/*
		 * set idle only if no cancel refs the server
		 * otherwise it will continue to be ACTIVE - no clients can
		 * acquire this server
		 */
		od_server_set_pool_state(server, OD_SERVER_IDLE);
	}
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

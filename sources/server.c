/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>
#include <odyssey.h>

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

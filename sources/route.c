
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <global.h>
#include <router.h>
#include <server.h>
#include <backend.h>
#include <rules.h>
#include <server.h>
#include <router.h>

od_multi_pool_element_t *
od_route_get_server_pool_element_locked(od_route_t *route,
					const od_address_t *address)
{
	od_multi_pool_key_t pool_key;
	memset(&pool_key, 0, sizeof(od_multi_pool_key_t));
	memcpy(&pool_key.address, address, sizeof(od_address_t));
	if (od_route_has_shared_pool(route)) {
		pool_key.dbname = route->id.database;
		pool_key.username = route->id.user;
	}

	return od_multi_pool_get_or_create_locked(od_route_server_pools(route),
						  &pool_key);
}

static inline int pool_next_idle_exclusive_locked(od_route_t *route,
						  const od_address_t *address,
						  od_server_t **server)
{
	assert(route->exclusive_pool != NULL);
	assert(route->shared_pool == NULL);

	od_multi_pool_element_t *pool_element =
		od_route_get_server_pool_element_locked(route, address);
	if (pool_element == NULL) {
		return OD_ROUTER_ERROR;
	}

	*server = od_pg_server_pool_next(&pool_element->pool, OD_SERVER_IDLE);

	return OD_ROUTER_OK;
}

static inline int pool_next_idle_shared_locked(od_route_t *route,
					       const od_address_t *address,
					       od_server_t **server)
{
	assert(route->exclusive_pool == NULL);
	assert(route->shared_pool != NULL);

	od_multi_pool_element_t *pool_element =
		od_route_get_server_pool_element_locked(route, address);
	if (pool_element == NULL) {
		return OD_ROUTER_ERROR;
	}

	*server = od_pg_server_pool_next(&pool_element->pool, OD_SERVER_IDLE);

	if (*server != NULL) {
		return OD_ROUTER_OK;
	}

	/*
	 * maybe we can replace some other mpool key's IDLE server?
	 */
	*server = od_multi_pool_peek_any_locked(route->shared_pool->mpool,
						OD_SERVER_IDLE);
	if (*server == NULL) {
		/* no idle servers at all */
		return OD_ROUTER_OK;
	}

	assert((*server)->pool_element != pool_element);

	/*
	 * the server can be replaced
	 * - create new server, set it as IDLE
	 * - close replaced
	 * - TODO: do not do this with lock on route held
	 * - TODO: maybe do not use some other's IDLE
	 *   if we have capacity to create new?
	 */

	od_server_t *new_server = od_server_allocate(
		route->rule->pool->reserve_prepared_statement);
	if (server == NULL) {
		return OD_ROUTER_ERROR;
	}
	od_id_generate(&new_server->id, "s");
	new_server->global = od_global_get();
	new_server->route = route;
	new_server->pool_element = pool_element;

	od_backend_close_connection(*server);
	od_server_set_pool_state(*server, OD_SERVER_UNDEF);
	od_server_free(*server);

	od_server_set_pool_state(new_server, OD_SERVER_IDLE);
	*server = new_server;

	return OD_ROUTER_OK;
}

int od_route_server_pool_next_idle_locked(od_route_t *route,
					  const od_address_t *address,
					  od_server_t **server)
{
	*server = NULL;

	int rc;

	if (od_route_has_exclusive_pool(route)) {
		rc = pool_next_idle_exclusive_locked(route, address, server);
	} else {
		rc = pool_next_idle_shared_locked(route, address, server);
	}

	return rc;
}

static inline int filter_by_address(void *arg, const od_multi_pool_key_t *key)
{
	const od_address_t *address = arg;

	if (od_address_cmp(address, &key->address) == 0) {
		return 1;
	}

	return 0;
}

int od_route_server_pool_total(od_route_t *route, od_multi_pool_element_t *el)
{
	if (od_route_has_exclusive_pool(route)) {
		return od_server_pool_total(&el->pool);
	}

	int total = od_multi_pool_total_locked(
		route->shared_pool->mpool, filter_by_address, &el->key.address);

	return total;
}

int od_route_server_pool_can_add_locked(od_route_t *route,
					od_multi_pool_element_t *el)
{
	if (od_route_has_exclusive_pool(route)) {
		return od_rule_pool_can_add(route->rule->pool,
					    od_server_pool_total(&el->pool));
	}

	int total = od_multi_pool_total_locked(
		route->shared_pool->mpool, filter_by_address, &el->key.address);

	/* shared pool can't have size of 0 */
	return total < route->shared_pool->pool_size;
}

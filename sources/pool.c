/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

od_rule_pool_t *od_rule_pool_alloc()
{
	od_rule_pool_t *pool;
	pool = od_malloc(sizeof(od_rule_pool_t));

	if (pool == NULL) {
		return NULL;
	}
	memset(pool, 0, sizeof(od_rule_pool_t));

	pool->discard = 1;
	pool->smart_discard = 0;
	pool->discard_query = NULL;
	pool->cancel = 1;
	pool->rollback = 1;

	return pool;
}

int od_rule_pool_free(od_rule_pool_t *pool)
{
	if (pool->routing_type) {
		od_free(pool->routing_type);
	}
	if (pool->pool_type_str) {
		od_free(pool->pool_type_str);
	}
	if (pool->discard_query) {
		od_free(pool->discard_query);
	}
	od_free(pool);
	return OK_RESPONSE;
}

int od_rule_pool_compare(od_rule_pool_t *a, od_rule_pool_t *b)
{
	/* pool */
	if (a->pool_type != b->pool_type)
		return 0;

	/* pool routing */
	if (a->routing != b->routing)
		return 0;

	/* size */
	if (a->size != b->size)
		return 0;

	/* timeout */
	if (a->timeout != b->timeout)
		return 0;

	/* ttl */
	if (a->ttl != b->ttl)
		return 0;

	/* pool_discard */
	if (a->discard != b->discard)
		return 0;

	/* cancel */
	if (a->cancel != b->cancel)
		return 0;

	/* rollback*/
	if (a->rollback != b->rollback)
		return 0;

	/* client idle timeout */
	if (a->client_idle_timeout != b->client_idle_timeout) {
		return 0;
	}

	/* idle_in_transaction_timeout */
	if (a->idle_in_transaction_timeout != b->idle_in_transaction_timeout) {
		return 0;
	}

	/* reserve_prepared_statement */
	if (a->reserve_prepared_statement != b->reserve_prepared_statement) {
		return 0;
	}

	return 1;
}

int od_rule_matches_client(od_rule_pool_t *pool, od_pool_client_type_t t)
{
	switch (t) {
	case OD_POOL_CLIENT_INTERNAL:
		return pool->routing == OD_RULE_POOL_INTERNAL;
	case OD_POOL_CLIENT_EXTERNAL:
		return pool->routing == OD_RULE_POOL_CLIENT_VISIBLE;
	default:
		// no matches
		return 0;
	}
}

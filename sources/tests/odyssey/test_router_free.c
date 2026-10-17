/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <router.h>
#include <tests/odyssey_test.h>

void odyssey_test_router_free_before_startup(void)
{
	od_router_t router;
	od_router_init(&router, NULL);

	/* Config errors must allow cleanup before any machine is started. */
	od_router_free(&router);
}

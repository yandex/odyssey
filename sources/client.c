/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium.h>
#include <odyssey.h>

machine_cond_t *od_client_get_io_cond(od_client_t *client)
{
	return client->io_cond;
}

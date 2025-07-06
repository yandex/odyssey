/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

od_global_t *current_global od_read_mostly = NULL;

void od_global_set(od_global_t *global)
{
	current_global = global;
}

od_global_t *od_global_get()
{
	return current_global;
}
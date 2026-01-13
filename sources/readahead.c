/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machine.h>
#include <machinarium/ds/vrb.h>

#include <global.h>
#include <instance.h>
#include <readahead.h>

int od_readahead_prepare(od_readahead_t *readahead)
{
	od_instance_t *instance = od_global_get_instance();
	size_t size = (size_t)instance->config.readahead;

	readahead->buf = mm_virtual_rbuf_create(size);
	if (readahead->buf == NULL) {
		return -1;
	}
	return 0;
}

void od_readahead_free(od_readahead_t *readahead)
{
	if (readahead->buf) {
		mm_virtual_rbuf_cache_put(&vrb_cache, readahead->buf);
	}
}

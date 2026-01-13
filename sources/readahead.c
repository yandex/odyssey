/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machine.h>
#include <machinarium/ds/vrb.h>

#include <readahead.h>

int od_readahead_prepare(od_readahead_t *readahead, int size)
{
	readahead->buf = mm_virtual_rbuf_create((size_t)size);
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

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <pthread.h>

#include <machinarium/machine.h>
#include <machinarium/ds/vrb.h>

#include <global.h>
#include <logger.h>
#include <instance.h>
#include <readahead.h>

static mm_virtual_rbuf_cache_t vrb_cache;
static pthread_once_t vrb_cache_init_ctrl = PTHREAD_ONCE_INIT;

static void destroy_vrb_cache(void)
{
	mm_virtual_rbuf_cache_destroy(&vrb_cache);
}

static void init_vrb_cache(void)
{
	size_t size = (size_t)od_global_get_instance()->config.cache_coroutine;
	int rc = mm_virtual_rbuf_cache_init(&vrb_cache, size);
	if (rc) {
		od_gfatal("readahead", NULL, NULL,
			  "can't create vrb cache, errno = %d (%s)",
			  machine_errno(), strerror(machine_errno()));
		abort();
	}

	atexit(destroy_vrb_cache);
}

int od_readahead_prepare(od_readahead_t *readahead)
{
	pthread_once(&vrb_cache_init_ctrl, init_vrb_cache);

	readahead->buf = mm_virtual_rbuf_cache_get(&vrb_cache);
	if (readahead->buf != NULL) {
		return 0;
	}

	od_instance_t *instance = od_global_get_instance();
	size_t size = (size_t)instance->config.readahead;

	readahead->buf = mm_virtual_rbuf_create(size);
	if (readahead->buf != NULL) {
		return 0;
	}

	return -1;
}

void od_readahead_free(od_readahead_t *readahead)
{
	if (readahead->buf) {
		mm_virtual_rbuf_cache_put(&vrb_cache, readahead->buf);
	}
}

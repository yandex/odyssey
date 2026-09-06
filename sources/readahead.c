/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <arpa/inet.h>

#include <machinarium/machine.h>
#include <machinarium/ds/vrb.h>
#include <kiwi/header.h>

#include <global.h>
#include <logger.h>
#include <instance.h>
#include <readahead.h>

/*
 * Per-thread vrb cache.
 *
 * Each machinarium thread (worker, od_system, etc.) gets its own cache
 * instance, eliminating spinlock contention on the hot read/return path.
 *
 * Because od_readahead_prepare() is now only called from worker threads
 * (for both client and server io), every buffer is obtained from and
 * returned to the same per-thread cache — no cross-thread ownership transfer.
 */

static OD_THREAD_LOCAL mm_virtual_rbuf_cache_t *tls_vrb_cache = NULL;
static OD_THREAD_LOCAL int tls_vrb_cache_initialized = 0;

static void destroy_vrb_cache(void *arg)
{
	mm_virtual_rbuf_cache_t *cache = (mm_virtual_rbuf_cache_t *)arg;
	if (cache == NULL) {
		return;
	}

	mm_virtual_rbuf_cache_destroy(cache);

	od_free(cache);

	tls_vrb_cache = NULL;
	tls_vrb_cache_initialized = 0;
}

static mm_virtual_rbuf_cache_t *get_vrb_cache(void)
{
	if (tls_vrb_cache_initialized) {
		return tls_vrb_cache;
	}

	tls_vrb_cache_initialized = 1;

	mm_virtual_rbuf_cache_t *cache =
		od_malloc(sizeof(mm_virtual_rbuf_cache_t));
	if (cache == NULL) {
		return NULL;
	}

	size_t size = (size_t)od_global_get_instance()->config.cache_coroutine;
	int rc = mm_virtual_rbuf_cache_init(cache, size);
	if (rc) {
		od_free(cache);
		tls_vrb_cache = NULL;
		od_gfatal("readahead", NULL, NULL,
			  "can't create vrb cache, errno = %d (%s)",
			  machine_errno(), strerror(machine_errno()));
		return NULL;
	}

	tls_vrb_cache = cache;

	/* destroy when this machinarium thread exits */
	mm_machine_atexit(destroy_vrb_cache, cache);

	return cache;
}

int od_readahead_prepare(od_readahead_t *readahead)
{
	mm_virtual_rbuf_cache_t *cache = get_vrb_cache();
	if (cache == NULL) {
		return -1;
	}

	readahead->buf = mm_virtual_rbuf_cache_get(cache);
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
		mm_virtual_rbuf_cache_t *cache = get_vrb_cache();
		if (cache != NULL) {
			mm_virtual_rbuf_cache_put(cache, readahead->buf);
		} else {
			mm_virtual_rbuf_free(readahead->buf);
		}
	}
}

void od_readahead_describe(od_readahead_t *readahead, char *buf,
			   size_t buf_size)
{
	if (buf_size == 0) {
		return;
	}

	buf[0] = '\0';

	int unread = od_readahead_unread(readahead);
	struct iovec rvec = od_readahead_read_begin(readahead);

	int written =
		snprintf(buf, buf_size, "readahead: %d bytes unread", unread);
	if (written < 0 || (size_t)written >= buf_size) {
		return;
	}

	const uint8_t *data = (const uint8_t *)rvec.iov_base;
	size_t avail = rvec.iov_len;
	size_t pos = 0;
	int msg_count = 0;

	while (pos + sizeof(kiwi_header_t) <= avail && msg_count < 10) {
		uint8_t type = data[pos];
		uint32_t len;
		memcpy(&len, data + pos + 1, sizeof(len));
		len = ntohl(len);

		if (len < 4) {
			break;
		}

		int w = snprintf(buf + written, buf_size - (size_t)written,
				 "%s%c(len=%u)",
				 msg_count == 0 ? "; msgs: " : ", ",
				 (type >= 32 && type < 127) ? (char)type : '?',
				 len);
		if (w < 0 || (size_t)w >= buf_size - (size_t)written) {
			break;
		}
		written += w;
		msg_count++;

		pos += 1 + (size_t)len;
	}
}

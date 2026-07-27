/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <pthread.h>

#include <machinarium/machine.h>
#include <machinarium/ds/vrb.h>
#include <kiwi/header.h>

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

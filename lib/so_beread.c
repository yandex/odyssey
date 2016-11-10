
/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <so_macro.h>
#include <so_stream.h>
#include <so_header.h>
#include <so_beread.h>

int so_beread_startup(sobestartup_t *su, uint8_t *data, uint32_t size)
{
	uint32_t pos_size = size;
	uint8_t *pos = data;
	int rc;
	uint32_t len;
	rc = so_stream_read32(&len, &pos, &pos_size);
	if (so_unlikely(rc == -1))
		return -1;
	uint32_t version;
	rc = so_stream_read32(&version, &pos, &pos_size);
	if (so_unlikely(rc == -1))
		return -1;
	switch (version) {
	/* StartupMessage */
	case 196608:
		su->is_cancel = 0;
		break;
	/* CancelRequest */
	case 80877102: {
		su->is_cancel = 1;
		rc = so_stream_read32(&su->key_pid, &pos, &pos_size);
		if (so_unlikely(rc == -1))
		rc = so_stream_read32(&su->key, &pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		break;
	}
	default:
		return -1;
	}
	return 0;
}


/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <flint.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_log.h"
#include "od_io.h"

int od_read(ftio_t *io, sostream_t *stream)
{
	so_stream_reset(stream);
	for (;;) {
		uint32_t pos_size = so_stream_used(stream);
		uint8_t *pos_data = stream->s;
		uint32_t len;
		int to_read;
		to_read = so_read(&len, &pos_data, &pos_size);
		if (to_read == 0)
			break;
		if (to_read == -1)
			return -1;
		int rc = so_stream_ensure(stream, to_read);
		if (rc == -1)
			return -1;
		rc = ft_read(io, to_read, 0);
		if (rc < 0)
			return -1;
		char *data_pointer = ft_read_buf(io);
		memcpy(stream->p, data_pointer, to_read);
		so_stream_advance(stream, to_read);
	}
	return 0;
}

int od_write(ftio_t *io, sostream_t *stream)
{
	int rc;
	rc = ft_write(io, (char*)stream->s, so_stream_used(stream), 0);
	if (rc < 0)
		return -1;
	return 0;
}

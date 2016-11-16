
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
#include <so_key.h>
#include <so_read.h>
#include <so_feread.h>

int so_feread_ready(int *status, uint8_t *data, uint32_t size)
{
	soheader_t *header = (soheader_t*)data;
	uint32_t len;
	int rc = so_read(&len, &data, &size);
	if (so_unlikely(rc != 0))
		return -1;
	if (so_unlikely(header->type != 'Z' || len != 1))
		return -1;
	*status = header->data[0];
	return 0;
}

int so_feread_key(sokey_t *key, uint8_t *data, uint32_t size)
{
	soheader_t *header = (soheader_t*)data;
	uint32_t len;
	int rc = so_read(&len, &data, &size);
	if (so_unlikely(rc != 0))
		return -1;
	if (so_unlikely(header->type != 'K' || len != 8))
		return -1;
	uint32_t pos_size = len;
	uint8_t *pos = header->data;
	rc = so_stream_read32(&key->key_pid, &pos, &pos_size);
	if (so_unlikely(rc == -1))
		return -1;
	rc = so_stream_read32(&key->key, &pos, &pos_size);
	if (so_unlikely(rc == -1))
		return -1;
	return 0;
}

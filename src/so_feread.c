
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
#include <so_parameter.h>
#include <so_read.h>
#include <so_feread.h>

int so_feread_ready(int *status, uint8_t *data, uint32_t size)
{
	so_header_t *header = (so_header_t*)data;
	uint32_t len;
	int rc = so_read(&len, &data, &size);
	if (so_unlikely(rc != 0))
		return -1;
	if (so_unlikely(header->type != 'Z' || len != 1))
		return -1;
	*status = header->data[0];
	return 0;
}

int so_feread_key(so_key_t *key, uint8_t *data, uint32_t size)
{
	so_header_t *header = (so_header_t*)data;
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

int so_feread_auth(uint32_t *type, uint8_t salt[4], uint8_t *data, uint32_t size)
{
	so_header_t *header = (so_header_t*)data;
	uint32_t len;
	int rc = so_read(&len, &data, &size);
	if (so_unlikely(rc != 0))
		return -1;
	if (so_unlikely(header->type != 'R'))
		return -1;
	uint32_t pos_size = len;
	uint8_t *pos = header->data;
	rc = so_stream_read32(type, &pos, &pos_size);
	if (so_unlikely(rc == -1))
		return -1;
	switch (*type) {
	/* AuthenticationOk */
	case 0:
		return 0;
	/* AuthenticationCleartextPassword */
	case 3:
		return 0;
	/* AuthenticationMD5Password */
	case 5:
		if (pos_size != 4)
			return -1;
		memcpy(salt, pos, 4);
		return 0;
	}
	/* unsupported */
	return -1;
}

int so_feread_parameter(so_parameters_t *params, uint8_t *data, uint32_t size)
{
	so_header_t *header = (so_header_t*)data;
	uint32_t len;
	int rc = so_read(&len, &data, &size);
	if (so_unlikely(rc != 0))
		return -1;
	if (so_unlikely(header->type != 'S'))
		return -1;
	/* name */
	uint32_t name_len;
	uint8_t *name;
	name = data;
	rc = so_stream_readsz(&data, &name_len);
	if (so_unlikely(rc == -1))
		return -1;
	name_len = data - name;
	/* value */
	uint32_t value_len;
	uint8_t *value;
	value = data;
	rc = so_stream_readsz(&data, &value_len);
	if (so_unlikely(rc == -1))
		return -1;
	value_len = data - value;
	rc = so_parameters_add(params, name, name_len, value, value_len);
	if (so_unlikely(rc == -1))
		return -1;
	return 0;
}

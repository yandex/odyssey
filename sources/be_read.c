
/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "sources/macro.h"
#include "sources/stream.h"
#include "sources/header.h"
#include "sources/key.h"
#include "sources/password.h"
#include "sources/parameter.h"
#include "sources/be_read.h"
#include "sources/be_write.h"
#include "sources/read.h"

static inline int
so_beread_options(so_bestartup_t *su, char *pos, uint32_t pos_size)
{
	for (;;) {
		/* name */
		uint32_t name_size;
		char *name = pos;
		int rc;
		rc = so_stream_readsz(&pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		name_size = pos - name;
		if (name_size == 1)
			break;
		/* value */
		uint32_t value_size;
		char *value = pos;
		rc = so_stream_readsz(&pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		value_size = pos - value;
		rc = so_parameters_add(&su->params, name, name_size,
		                       value, value_size);
		if (rc == -1)
			return -1;
	}

	/* set common parameters */
	so_parameter_t *param = (so_parameter_t*)su->params.buf.start;
	so_parameter_t *end = (so_parameter_t*)su->params.buf.pos;
	while (param < end) {
		if (param->name_len == 5 && memcmp(param->data, "user", 5) == 0) {
			su->user = param;
		} else
		if (param->name_len == 9 && memcmp(param->data, "database", 9) == 0) {
			su->database = param;
		} else
		if (param->name_len == 18 && memcmp(param->data, "application_name", 18) == 0) {
			su->application_name = param;
		}
		param = so_parameter_next(param);
	}

	/* user is mandatory */
	if (su->user == NULL)
		return -1;
	if (su->database == NULL)
		su->database = su->user;
	return 0;
}

int so_beread_startup(so_bestartup_t *su, char *data, uint32_t size)
{
	uint32_t pos_size = size;
	char *pos = data;
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
		rc = so_beread_options(su, pos, pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		break;
	/* CancelRequest */
	case 80877102:
		su->is_cancel = 1;
		rc = so_stream_read32(&su->key.key_pid, &pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		rc = so_stream_read32(&su->key.key, &pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		break;
	/* SSLRequest */
	case  80877103:
		su->is_ssl_request = 1;
		break;
	default:
		return -1;
	}
	return 0;
}

int so_beread_password(so_password_t *pw, char *data, uint32_t size)
{
	so_header_t *header = (so_header_t*)data;
	uint32_t len;
	int rc = so_read(&len, &data, &size);
	if (so_unlikely(rc != 0))
		return -1;
	if (so_unlikely(header->type != 'p'))
		return -1;
	pw->password_len = len;
	pw->password = malloc(len);
	if (pw->password == NULL)
		return -1;
	memcpy(pw->password, header->data, len);
	return 0;
}

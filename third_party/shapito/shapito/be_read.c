
/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

#include "shapito.h"

static inline int
shapito_be_read_options(shapito_be_startup_t *su, char *pos, uint32_t pos_size)
{
	for (;;) {
		/* name */
		uint32_t name_size;
		char *name = pos;
		int rc;
		rc = shapito_stream_readsz(&pos, &pos_size);
		if (shapito_unlikely(rc == -1))
			return -1;
		name_size = pos - name;
		if (name_size == 1)
			break;
		/* value */
		uint32_t value_size;
		char *value = pos;
		rc = shapito_stream_readsz(&pos, &pos_size);
		if (shapito_unlikely(rc == -1))
			return -1;
		value_size = pos - value;
		rc = shapito_parameters_add(&su->params, name, name_size,
		                            value, value_size);
		if (rc == -1)
			return -1;
	}

	/* set common parameters */
	shapito_parameter_t *param;
	param = (shapito_parameter_t*)su->params.buf.start;
	shapito_parameter_t *end;
	end = (shapito_parameter_t*)su->params.buf.pos;
	while (param < end) {
		if (param->name_len == 5 && memcmp(param->data, "user", 5) == 0) {
			su->user = param;
		} else
		if (param->name_len == 9 && memcmp(param->data, "database", 9) == 0) {
			su->database = param;
		} else
		if (param->name_len == 17 && memcmp(param->data, "application_name", 17) == 0) {
			su->application_name = param;
		}
		param = shapito_parameter_next(param);
	}

	/* user is mandatory */
	if (su->user == NULL)
		return -1;
	if (su->database == NULL)
		su->database = su->user;
	return 0;
}

SHAPITO_API int
shapito_be_read_startup(shapito_be_startup_t *su, char *data, uint32_t size)
{
	uint32_t pos_size = size;
	char *pos = data;
	int rc;
	uint32_t len;
	rc = shapito_stream_read32(&len, &pos, &pos_size);
	if (shapito_unlikely(rc == -1))
		return -1;
	uint32_t version;
	rc = shapito_stream_read32(&version, &pos, &pos_size);
	if (shapito_unlikely(rc == -1))
		return -1;
	switch (version) {
	/* StartupMessage */
	case 196608:
		su->is_cancel = 0;
		rc = shapito_be_read_options(su, pos, pos_size);
		if (shapito_unlikely(rc == -1))
			return -1;
		break;
	/* CancelRequest */
	case 80877102:
		su->is_cancel = 1;
		rc = shapito_stream_read32(&su->key.key_pid, &pos, &pos_size);
		if (shapito_unlikely(rc == -1))
			return -1;
		rc = shapito_stream_read32(&su->key.key, &pos, &pos_size);
		if (shapito_unlikely(rc == -1))
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

SHAPITO_API int
shapito_be_read_password(shapito_password_t *pw, char *data, uint32_t size)
{
	shapito_header_t *header = (shapito_header_t*)data;
	uint32_t len;
	int rc = shapito_read(&len, &data, &size);
	if (shapito_unlikely(rc != 0))
		return -1;
	if (shapito_unlikely(header->type != SHAPITO_FE_PASSWORD_MESSAGE))
		return -1;
	pw->password_len = len;
	pw->password = malloc(len);
	if (pw->password == NULL)
		return -1;
	memcpy(pw->password, header->data, len);
	return 0;
}

SHAPITO_API int
shapito_be_read_query(char **query, uint32_t *query_len, char *data, uint32_t size)
{
	shapito_header_t *header = (shapito_header_t*)data;
	uint32_t len;
	int rc = shapito_read(&len, &data, &size);
	if (shapito_unlikely(rc != 0))
		return -1;
	if (shapito_unlikely(header->type != SHAPITO_FE_QUERY))
		return -1;
	*query = header->data;
	*query_len = len;
	return 0;
}


/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

#include "shapito.h"

int shapito_parameters_add(shapito_parameters_t *params,
                           char *name,
                           uint32_t name_len,
                           char *value,
                           uint32_t value_len)
{
	int size = sizeof(shapito_parameter_t) + name_len + value_len;
	int rc;
	rc = shapito_stream_ensure(&params->buf, size);
	if (shapito_unlikely(rc == -1))
		return -1;
	shapito_parameter_t *param;
	param = (shapito_parameter_t*)params->buf.pos;
	param->name_len = name_len;
	param->value_len = value_len;
	memcpy(param->data, name, name_len);
	memcpy(param->data + name_len, value, value_len);
	shapito_stream_advance(&params->buf, size);
	return 0;
}

int shapito_parameters_update(shapito_parameters_t *params,
                              char *name,
                              uint32_t name_len,
                              char *value,
                              uint32_t value_len)
{
	shapito_parameters_t update;
	shapito_parameters_init(&update);

	shapito_parameter_t *param;
	shapito_parameter_t *end;
	param = (shapito_parameter_t*)params->buf.start;
	end = (shapito_parameter_t*)params->buf.pos;

	int rc;
	int found = 0;
	while (param < end) {
		if (param->name_len == (uint32_t)name_len) {
			if (strncasecmp(shapito_parameter_name(param), name, name_len) == 0) {
				rc = shapito_parameters_add(&update, name, name_len,
				                            value, value_len);
				if (rc == -1) {
					shapito_parameters_free(&update);
					return -1;
				}
				found = 1;
				param = shapito_parameter_next(param);
				continue;
			}
		}
		rc = shapito_parameters_add(&update,
		                            shapito_parameter_name(param),
		                            param->name_len,
		                            shapito_parameter_value(param),
		                            param->value_len);
		if (rc == -1) {
			shapito_parameters_free(&update);
			return -1;
		}
		param = shapito_parameter_next(param);
	}

	if (! found) {
		rc = shapito_parameters_add(&update, name, name_len,
		                            value, value_len);
		if (rc == -1) {
			shapito_parameters_free(&update);
			return -1;
		}
	}

	shapito_parameters_free(params);
	*params = update;
	return 0;
}

int shapito_parameters_merge(shapito_parameters_t *merge,
                             shapito_parameters_t *a,
                             shapito_parameters_t *b)
{
	shapito_parameter_t *param;
	shapito_parameter_t *end;
	param = (shapito_parameter_t*)b->buf.start;
	end = (shapito_parameter_t*)b->buf.pos;
	while (param < end) {
		int rc;
		rc = shapito_parameters_add(merge,
		                            shapito_parameter_name(param),
		                            param->name_len,
		                            shapito_parameter_value(param),
		                            param->value_len);
		if (rc == -1)
			return -1;
		param = shapito_parameter_next(param);
	}

	param = (shapito_parameter_t*)a->buf.start;
	end = (shapito_parameter_t*)a->buf.pos;
	while (param < end) {
		int rc;
		rc = shapito_parameters_update(merge,
		                               shapito_parameter_name(param),
		                               param->name_len,
		                               shapito_parameter_value(param),
		                               param->value_len);
		if (rc == -1)
			return -1;
		param = shapito_parameter_next(param);
	}
	return 0;
}

shapito_parameter_t*
shapito_parameters_find(shapito_parameters_t *params, char *name, int name_len)
{
	shapito_parameter_t *param;
	param = (shapito_parameter_t*)params->buf.start;
	shapito_parameter_t *end;
	end = (shapito_parameter_t*)params->buf.pos;
	while (param < end) {
		if (param->name_len == (uint32_t)name_len) {
			if (strncasecmp(shapito_parameter_name(param), name, name_len) == 0)
				return param;
		}
		param = shapito_parameter_next(param);
	}
	return NULL;
}

int
shapito_parameter_quote(char *src, char *dst, int dst_len)
{
	assert(dst_len >= 4);
	char *pos = dst;
	char *end = dst + dst_len - 4;
	*pos++ = 'E';
	*pos++ = '\'';
	while (*src && pos < end) {
		if (*src == '\'')
			*pos++ = '\'';
		else
		if (*src == '\\') {
			*dst++ = '\\';
		}
		*pos++ = *src++;
	}
	if (*src || pos > end)
		return -1;
	*pos++ = '\'';
	*pos = 0;
	return 0;
}

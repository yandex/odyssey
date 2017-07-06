
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


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
#include <so_parameter.h>

int so_parameters_add(so_parameters_t *params,
                      uint8_t *name,
                      uint32_t name_len,
                      uint8_t *value,
                      uint32_t value_len)
{
	int size = sizeof(so_parameter_t) + name_len + value_len;
	int rc;
	rc = so_stream_ensure(&params->buf, size);
	if (so_unlikely(rc == -1))
		return -1;
	so_parameter_t *param;
	param = (so_parameter_t*)params->buf.p;
	param->name_len = name_len;
	param->value_len = value_len;
	memcpy(param->data, name, name_len);
	memcpy(param->data + name_len, value, value_len);
	so_stream_advance(&params->buf, size);
	return 0;
}

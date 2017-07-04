#ifndef SO_PARAMETER_H_
#define SO_PARAMETER_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_parameter  so_parameter_t;
typedef struct so_parameters so_parameters_t;

struct so_parameter
{
	uint32_t name_len;
	uint32_t value_len;
	char data[];
};

struct so_parameters
{
	so_stream_t buf;
};

static inline char*
so_parameter_name(so_parameter_t *param)
{
	return param->data;
}

static inline char*
so_parameter_value(so_parameter_t *param)
{
	return param->data + param->name_len;
}

static inline so_parameter_t*
so_parameter_next(so_parameter_t *param)
{
	return (so_parameter_t*)(param->data + param->name_len +
	                         param->value_len);
}

static inline void
so_parameters_init(so_parameters_t *params)
{
	so_stream_init(&params->buf);
}

static inline void
so_parameters_free(so_parameters_t *params)
{
	so_stream_free(&params->buf);
}

int so_parameters_add(so_parameters_t*, char*, uint32_t,
                      char*, uint32_t);

#endif

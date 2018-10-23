#ifndef KIWI_BE_READ_H
#define KIWI_BE_READ_H

/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
*/

typedef struct kiwi_be_startup kiwi_be_startup_t;

struct kiwi_be_startup
{
	int            is_ssl_request;
	int            is_cancel;
	kiwi_key_t     key;
	kiwi_params_t  params;
	kiwi_param_t  *user;
	kiwi_param_t  *database;
	kiwi_param_t  *application_name;
};

static inline void
kiwi_be_startup_init(kiwi_be_startup_t *su)
{
	su->is_cancel        = 0;
	su->is_ssl_request   = 0;
	su->user             = NULL;
	su->database         = NULL;
	su->application_name = NULL;
	kiwi_params_init(&su->params);
	kiwi_key_init(&su->key);
}

static inline void
kiwi_be_startup_free(kiwi_be_startup_t *su)
{
	kiwi_params_free(&su->params);
}

static inline int
kiwi_be_read_options(kiwi_be_startup_t *su, char *pos, uint32_t pos_size)
{
	for (;;) {
		/* name */
		uint32_t name_size;
		char *name = pos;
		int rc;
		rc = kiwi_readsz(&pos, &pos_size);
		if (kiwi_unlikely(rc == -1))
			return -1;
		name_size = pos - name;
		if (name_size == 1)
			break;
		/* value */
		uint32_t value_size;
		char *value = pos;
		rc = kiwi_readsz(&pos, &pos_size);
		if (kiwi_unlikely(rc == -1))
			return -1;
		value_size = pos - value;
		kiwi_param_t *param;
		param = kiwi_param_allocate(name, name_size, value, value_size);
		if (param == NULL)
			return -1;
		kiwi_params_add(&su->params, param);
	}

	/* set common params */
	kiwi_param_t *param = su->params.list;
	while (param)
	{
		if (param->name_len == 5 && !memcmp(param->data, "user", 5))
			su->user = param;
		else
		if (param->name_len == 9 && !memcmp(param->data, "database", 9))
			su->database = param;
		else
		if (param->name_len == 17 && !memcmp(param->data, "application_name", 17))
			su->application_name = param;
		param = param->next;
	}

	/* user is mandatory */
	if (su->user == NULL)
		return -1;
	if (su->database == NULL)
		su->database = su->user;
	return 0;
}

KIWI_API static inline int
kiwi_be_read_startup(machine_msg_t *msg, kiwi_be_startup_t *su)
{
	char *data;
	data = machine_msg_get_data(msg);
	uint32_t size;
	size = machine_msg_get_size(msg);

	uint32_t pos_size = size;
	char *pos = data;
	int rc;
	uint32_t len;
	rc = kiwi_read32(&len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	uint32_t version;
	rc = kiwi_read32(&version, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	switch (version) {
	/* StartupMessage */
	case 196608:
		su->is_cancel = 0;
		rc = kiwi_be_read_options(su, pos, pos_size);
		if (kiwi_unlikely(rc == -1))
			return -1;
		break;
	/* CancelRequest */
	case 80877102:
		su->is_cancel = 1;
		rc = kiwi_read32(&su->key.key_pid, &pos, &pos_size);
		if (kiwi_unlikely(rc == -1))
			return -1;
		rc = kiwi_read32(&su->key.key, &pos, &pos_size);
		if (kiwi_unlikely(rc == -1))
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

KIWI_API static inline int
kiwi_be_read_password(machine_msg_t *msg, kiwi_password_t *pw)
{
	char *data;
	data = machine_msg_get_data(msg);
	uint32_t size;
	size = machine_msg_get_size(msg);

	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_FE_PASSWORD_MESSAGE))
		return -1;
	pw->password_len = len;
	pw->password = malloc(len);
	if (pw->password == NULL)
		return -1;
	memcpy(pw->password, kiwi_header_data(header), len);
	return 0;
}

KIWI_API static inline int
kiwi_be_read_query(machine_msg_t *msg, char **query, uint32_t *query_len)
{
	char *data;
	data = machine_msg_get_data(msg);
	uint32_t size;
	size = machine_msg_get_size(msg);

	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_FE_QUERY))
		return -1;
	*query = kiwi_header_data(header);
	*query_len = len;
	return 0;
}

KIWI_API static inline int
kiwi_be_read_parse(machine_msg_t *msg, char **name, uint32_t *name_len,
                   char **query, uint32_t *query_len)
{
	char *data;
	data = machine_msg_get_data(msg);
	uint32_t size;
	size = machine_msg_get_size(msg);

	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_FE_PARSE))
		return -1;
	uint32_t pos_size = len;
	char *pos = kiwi_header_data(header);
	/* operator_name */
	*name = pos;
	rc = kiwi_readsz(&pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	*name_len = pos - *name;
	/* query */
	*query = pos;
	rc = kiwi_readsz(&pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	*query_len = pos - *query;
	/* typec */
	/* u16 */
	/* typev */
	/* u32 */
	return 0;
}

#endif /* KIWI_BE_READ_H */

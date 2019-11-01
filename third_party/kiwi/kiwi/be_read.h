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
	int        is_ssl_request;
	int        is_cancel;
	kiwi_key_t key;
	kiwi_var_t user;
	kiwi_var_t database;
	kiwi_var_t replication;
};

static inline void
kiwi_be_startup_init(kiwi_be_startup_t *su)
{
	su->is_cancel      = 0;
	su->is_ssl_request = 0;
	kiwi_key_init(&su->key);
	kiwi_var_init(&su->user, NULL, 0);
	kiwi_var_init(&su->database, NULL, 0);
	kiwi_var_init(&su->replication, NULL, 0);
}

static inline int
kiwi_be_read_options(kiwi_be_startup_t *su, char *pos, uint32_t pos_size,
                     kiwi_vars_t *vars)
{
	for (;;)
	{
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

		/* set common params */
		if (name_size == 5 && !memcmp(name, "user", 5))
			kiwi_var_set(&su->user, KIWI_VAR_UNDEF, value, value_size);
		else
		if (name_size == 9 && !memcmp(name, "database", 9))
			kiwi_var_set(&su->database, KIWI_VAR_UNDEF, value, value_size);
		else
		if (name_size == 12 && !memcmp(name, "replication", 12))
			kiwi_var_set(&su->replication, KIWI_VAR_UNDEF, value, value_size);
		else
			kiwi_vars_update(vars, name, name_size, value, value_size);
	}

	/* user is mandatory */
	if (su->user.value_len == 0)
		return -1;

	/* database = user, if not specified */
	if (su->database.value_len == 0)
		kiwi_var_set(&su->database, KIWI_VAR_UNDEF,
		             su->user.value,
		             su->user.value_len);
	return 0;
}

KIWI_API static inline int
kiwi_be_read_startup(char *data, uint32_t size, kiwi_be_startup_t *su, kiwi_vars_t *vars)
{
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
		rc = kiwi_be_read_options(su, pos, pos_size, vars);
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
kiwi_be_read_password(char *data, uint32_t size, kiwi_password_t *pw)
{
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
kiwi_be_read_query(char *data, uint32_t size, char **query, uint32_t *query_len)
{
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
kiwi_be_read_parse(char *data, uint32_t size, char **name, uint32_t *name_len,
                   char **query, uint32_t *query_len)
{
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

KIWI_API static inline int
kiwi_be_read_authentication_sasl_initial(char *data, uint32_t size, 
					      		   		 char **mechanism, char **auth_data)
{
	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_FE_PASSWORD_MESSAGE))
		return -1;

	uint32_t pos_size = len;
	char *pos = kiwi_header_data(header);

	*mechanism = pos;
	rc = kiwi_readsz(&pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;

	uint32_t auth_data_len;
	rc = kiwi_read32(&auth_data_len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;

	*auth_data = pos;

	return 0;
}

KIWI_API static inline int
kiwi_be_read_authentication_sasl(char *data, uint32_t size, 
						         char **auth_data)
{
	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_FE_PASSWORD_MESSAGE))
		return -1;

	*auth_data = kiwi_header_data(header);

	return 0;
}

#endif /* KIWI_BE_READ_H */

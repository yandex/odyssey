#ifndef KIWI_FE_READ_H
#define KIWI_FE_READ_H

/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
*/

typedef struct kiwi_fe_error kiwi_fe_error_t;

struct kiwi_fe_error
{
	char *severity;
	char *code;
	char *message;
	char *detail;
	char *hint;
};

KIWI_API static inline int
kiwi_fe_read_ready(int *status, char *data, uint32_t size)
{
	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_BE_READY_FOR_QUERY || len != 1))
		return -1;
	*status = header->data[0];
	return 0;
}

KIWI_API static inline int
kiwi_fe_read_key(kiwi_key_t *key, char *data, uint32_t size)
{
	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_BE_BACKEND_KEY_DATA || len != 8))
		return -1;
	uint32_t pos_size = len;
	char *pos = header->data;
	rc = kiwi_read32(&key->key_pid, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	rc = kiwi_read32(&key->key, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	return 0;
}

KIWI_API static inline int
kiwi_fe_read_auth(uint32_t *type, char salt[4], char *data, uint32_t size)
{
	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_BE_AUTHENTICATION))
		return -1;
	uint32_t pos_size = len;
	char *pos = header->data;
	rc = kiwi_read32(type, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
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

KIWI_API static inline int
kiwi_fe_read_parameter(char *data, uint32_t size,
                       char **name, uint32_t *name_len,
                       char **value, uint32_t *value_len)
{
	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_BE_PARAMETER_STATUS))
		return -1;
	uint32_t pos_size = len;
	char *pos = header->data;
	/* name */
	*name = pos;
	rc = kiwi_readsz(&pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	*name_len = pos - *name;
	/* value */
	*value = pos;
	rc = kiwi_readsz(&pos, &pos_size);
	if (kiwi_unlikely(rc == -1))
		return -1;
	*value_len = pos - *value;
	return 0;
}

KIWI_API static inline int
kiwi_fe_read_error(kiwi_fe_error_t *error, char *data, uint32_t size)
{
	kiwi_header_t *header = (kiwi_header_t*)data;
	uint32_t len;
	int rc = kiwi_read(&len, &data, &size);
	if (kiwi_unlikely(rc != 0))
		return -1;
	if (kiwi_unlikely(header->type != KIWI_BE_ERROR_RESPONSE))
		return -1;
	memset(error, 0, sizeof(*error));
	uint32_t pos_size = len;
	char *pos = header->data;
	for (;;)
	{
		char type;
		int rc;
		rc = kiwi_read8(&type, &pos, &pos_size);
		if (kiwi_unlikely(rc == -1))
			return -1;
		switch (type) {
		/* severity */
		case 'S':
			error->severity = pos;
			rc = kiwi_readsz(&pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				return -1;
			break;
		/* sqlstate */
		case 'C':
			error->code = pos;
			rc = kiwi_readsz(&pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				return -1;
			break;
		/* message */
		case 'M':
			error->message = pos;
			rc = kiwi_readsz(&pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				return -1;
			break;
		/* detail */
		case 'D':
			error->detail = pos;
			rc = kiwi_readsz(&pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				return -1;
			break;
		/* hint */
		case 'H':
			error->hint = pos;
			rc = kiwi_readsz(&pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				return -1;
			break;
		/* end */
		case 0:
			return 0;
		default:
			rc = kiwi_readsz(&pos, &pos_size);
			if (kiwi_unlikely(rc == -1))
				return -1;
			break;
		}
	}
	return 0;
}

#endif /* KIWI_FE_READ_H */

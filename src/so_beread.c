
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
#include <so_password.h>
#include <so_parameter.h>
#include <so_beread.h>
#include <so_read.h>

static inline int
so_beread_options(so_bestartup_t *su, uint8_t *pos, uint32_t pos_size)
{
	struct {
		char *key;
		int   key_size;
		char *value;
		int   value_size;
	} argv[32];
	int argc = 0;

	int rc;
	for (;;) {
		if (so_unlikely(argc == 32))
			return -1;
		int size;
		uint8_t *start;
		uint8_t *end;
		/* key */
		start = pos;
		rc = so_stream_readsz(&pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		end = pos;
		size = end - start;
		if (size == 1)
			break;
		argv[argc].key = (char*)start;
		argv[argc].key_size = size;
		/* value */
		start = pos;
		rc = so_stream_readsz(&pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		end = pos;
		size = end - start;
		argv[argc].value = (char*)start;
		argv[argc].value_size = size;
		argc++;
	}

	/* match common options */
	int i = 0;
	for (; i < argc; i++) {
		if (argv[i].key_size == 5 && memcmp(argv[i].key, "user", 5) == 0) {
			su->user = strdup(argv[i].value);
			if (su->user == NULL)
				return -1;
			su->user_len = argv[i].value_size;
		} else
		if (argv[i].key_size == 9 && memcmp(argv[i].key, "database", 9) == 0) {
			su->database = strdup(argv[i].value);
			if (su->database == NULL)
				return -1;
			su->database_len = argv[i].value_size;
		} else
		if (argv[i].key_size == 18 && memcmp(argv[i].key, "application_name", 18) == 0) {
			su->application_name = strdup(argv[i].value);
			if (su->application_name == NULL)
				return -1;
			su->application_name_len = argv[i].value_size;
		}
	}

	/* user is mandatory */
	if (su->user == NULL)
		return -1;
	if (su->database == NULL) {
		su->database = strdup(su->user);
		if (su->database == NULL)
			return -1;
	}
	return 0;
}

int so_beread_startup(so_bestartup_t *su, uint8_t *data, uint32_t size)
{
	uint32_t pos_size = size;
	uint8_t *pos = data;
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
	case 80877102: {
		su->is_cancel = 1;
		rc = so_stream_read32(&su->key.key_pid, &pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		rc = so_stream_read32(&su->key.key, &pos, &pos_size);
		if (so_unlikely(rc == -1))
			return -1;
		break;
	}
	default:
		return -1;
	}
	return 0;
}

int so_beread_password(so_password_t *pw, uint8_t *data, uint32_t size)
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

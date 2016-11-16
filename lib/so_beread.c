
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
#include <so_beread.h>

void so_bestartup_init(sobestartup_t *su)
{
	su->is_cancel = 0;
	su->database = NULL;
	su->database_len = 0;
	su->user = NULL;
	su->user_len = 0;
	so_keyinit(&su->key);
}

void so_bestartup_free(sobestartup_t *su)
{
	if (su->database)
		free(su->database);
	if (su->user)
		free(su->user);
}

static inline int
so_beread_options(sobestartup_t *su, uint8_t *pos, uint32_t pos_size)
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
			su->user = argv[i].value;
			su->user_len = argv[i].value_size;
		} else
		if (argv[i].key_size == 9 && memcmp(argv[i].key, "database", 9) == 0) {
			su->database = argv[i].value;
			su->database_len = argv[i].value_size;
		}
	}

	/* user is mandatory */
	if (su->user == NULL)
		return -1;
	su->user = strdup(su->user);
	if (su->user == NULL)
		return -1;
	if (su->database)
		su->database = strdup(su->database);
	else
		su->database = strdup(su->user);
	if (su->database == NULL) {
		free(su->user);
		su->user = NULL;
		return -1;
	}
	return 0;
}

int so_beread_startup(sobestartup_t *su, uint8_t *data, uint32_t size)
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

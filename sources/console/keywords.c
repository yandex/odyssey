/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <console/keywords.h>
#include <console/parse.tab.h>

typedef struct {
	const char *name;
	int token;
} od_console_keyword_t;

static const od_console_keyword_t keywords[] = {
	/* top-level commands */
	{ "show", KW_SHOW },
	{ "kill_client", KW_KILL_CLIENT },
	{ "reload", KW_RELOAD },
	{ "pause", KW_PAUSE },
	{ "resume", KW_RESUME },
	{ "set", KW_SET },
	{ "drop", KW_DROP },
	{ "gc", KW_GC },

	/* drop targets */
	{ "servers", KW_SERVERS },

	/* set helpers */
	{ "to", KW_TO },
	{ "default", KW_DEFAULT },

	{ NULL, 0 }
};

int od_console_keyword_lookup(const char *str, size_t len)
{
	for (size_t i = 0; keywords[i].name != NULL; i++) {
		if (strlen(keywords[i].name) == len &&
		    strncasecmp(keywords[i].name, str, len) == 0) {
			return keywords[i].token;
		}
	}
	return 0;
}

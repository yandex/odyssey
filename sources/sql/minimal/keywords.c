/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <sql/minimal/keywords.h>
#include <sql/minimal/parse.tab.h>

typedef struct {
	const char *name;
	int token;
} od_sql_minimal_keyword_t;

static const od_sql_minimal_keyword_t keywords[] = {
	/* session / admin commands */
	{ "set", KW_SET },
	{ "show", KW_SHOW },
	{ "local", KW_LOCAL },
	{ "session", KW_SESSION },
	{ "to", KW_TO },
	{ "default", KW_DEFAULT },
	{ "all", KW_ALL },
	{ "true", KW_TRUE },
	{ "false", KW_FALSE },
	{ "on", KW_ON },
	{ "off", KW_OFF },
	{ "begin", KW_BEGIN },

	{ "kill_client", KW_KILL_CLIENT },
	{ "reload", KW_RELOAD },
	{ "pause", KW_PAUSE },
	{ "resume", KW_RESUME },
	{ "drop", KW_DROP },

	/* transaction control */
	{ "work", KW_WORK },
	{ "transaction", KW_TRANSACTION },

	{ "deallocate", KW_DEALLOCATE },
	{ "prepare", KW_PREPARE },

	{ "discard", KW_DISCARD },
	{ "temp", KW_TEMP },
	{ "temporary", KW_TEMPORARY },
	{ "plans", KW_PLANS },
	{ "sequences", KW_SEQUENCES },

	{ NULL, 0 }
};

int od_sql_minimal_keyword_lookup(const char *str, size_t len)
{
	for (size_t i = 0; keywords[i].name != NULL; i++) {
		if (strlen(keywords[i].name) == len &&
		    strncasecmp(keywords[i].name, str, len) == 0) {
			return keywords[i].token;
		}
	}
	return 0;
}

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <sql/minimal/keywords.h>
#include <sql/minimal/parse.tab.h>

/* length of the longest keyword, "transaction" */
#define OD_SQL_MINIMAL_KEYWORD_MAX_LEN 11

static inline int od_sql_minimal_keyword_eq(const char *kw, const char *str,
					    size_t len)
{
	for (size_t i = 0; i < len; i++) {
		char c = str[i];
		if (c >= 'A' && c <= 'Z') {
			c += 32;
		}
		if (kw[i] != c) {
			return 0;
		}
	}
	return 1;
}

#define KW(kwname, token)                                  \
	if (len == sizeof(kwname) - 1 &&                   \
	    od_sql_minimal_keyword_eq(kwname, str, len)) { \
		return token;                              \
	}

int od_sql_minimal_keyword_lookup(const char *str, size_t len)
{
	if (len == 0 || len > OD_SQL_MINIMAL_KEYWORD_MAX_LEN) {
		return 0;
	}

	switch (str[0] | 0x20) {
	case 'a':
		KW("all", KW_ALL);
		break;
	case 'b':
		KW("begin", KW_BEGIN);
		break;
	case 'd':
		KW("deallocate", KW_DEALLOCATE);
		KW("default", KW_DEFAULT);
		KW("discard", KW_DISCARD);
		break;
	case 'f':
		KW("false", KW_FALSE);
		break;
	case 'l':
		KW("local", KW_LOCAL);
		break;
	case 'o':
		KW("off", KW_OFF);
		KW("on", KW_ON);
		break;
	case 'p':
		KW("plans", KW_PLANS);
		KW("prepare", KW_PREPARE);
		break;
	case 's':
		KW("sequences", KW_SEQUENCES);
		KW("session", KW_SESSION);
		KW("set", KW_SET);
		KW("show", KW_SHOW);
		break;
	case 't':
		KW("temp", KW_TEMP);
		KW("temporary", KW_TEMPORARY);
		KW("to", KW_TO);
		KW("transaction", KW_TRANSACTION);
		KW("true", KW_TRUE);
		break;
	case 'u':
		KW("unlisten", KW_UNLISTEN);
		break;
	case 'w':
		KW("work", KW_WORK);
		break;
	default:
		break;
	}

	return 0;
}

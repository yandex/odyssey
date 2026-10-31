/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

#include <kiwi/kiwi.h>

#include <limits.h>

static inline int find_id_end(const char *str, int len, int pos)
{
	/*
	 * return the end position of id
	 *
	 * https://www.postgresql.org/docs/current/sql-syntax-lexical.html#SQL-SYNTAX-DOLLAR-QUOTING
	 * https://www.postgresql.org/docs/current/sql-syntax-lexical.html#SQL-SYNTAX-IDENTIFIERS
	 */

	static const char *id_start_alphabet =
		"qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_";
	static const char *identifier_alphabet =
		"qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_0123456789";

	if (pos >= len) {
		return pos;
	}

	/* must start with letter or _ */
	if (strchr(id_start_alphabet, str[pos]) == NULL) {
		return pos;
	}

	++pos;

	while (pos < len && strchr(identifier_alphabet, str[pos]) != NULL) {
		++pos;
	}

	return pos;
}

static int kiwi_search_path_is_safe(const char *str, int len)
{
	static const char *allowed_alphabet =
		"qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_0123456789,$ ";
	/* Conservatively treat PostgreSQL 18 reserved words as complex. Keep sorted. */
	static const char *const reserved[] = {
		"all",		"analyse",
		"analyze",	"and",
		"any",		"array",
		"as",		"asc",
		"asymmetric",	"both",
		"case",		"cast",
		"check",	"collate",
		"column",	"constraint",
		"create",	"current_catalog",
		"current_date", "current_role",
		"current_time", "current_timestamp",
		"current_user", "default",
		"deferrable",	"desc",
		"distinct",	"do",
		"else",		"end",
		"except",	"false",
		"fetch",	"for",
		"foreign",	"from",
		"grant",	"group",
		"having",	"in",
		"initially",	"intersect",
		"into",		"lateral",
		"leading",	"limit",
		"localtime",	"localtimestamp",
		"not",		"null",
		"offset",	"on",
		"only",		"or",
		"order",	"placing",
		"primary",	"references",
		"returning",	"select",
		"session_user", "some",
		"symmetric",	"system_user",
		"table",	"then",
		"to",		"trailing",
		"true",		"union",
		"unique",	"user",
		"using",	"variadic",
		"when",		"where",
		"window",	"with"
	};
	int i = 0;
	int end = len - 1;
	for (;;) {
		while (i < end && str[i] == ' ') {
			i++;
		}
		if (i >= end) {
			return 0;
		}
		int quoted = str[i] == '"';
		if (quoted) {
			i++;
		} else if (!((str[i] >= 'a' && str[i] <= 'z') ||
			     (str[i] >= 'A' && str[i] <= 'Z') ||
			     str[i] == '_')) {
			return 0;
		}
		int start = i;
		int name_len = 0;
		int closed = !quoted;
		while (i < end) {
			char c = str[i];
			if (quoted && c == '"') {
				if (i + 1 < end && str[i + 1] == '"') {
					i++;
				} else {
					i++;
					closed = 1;
					break;
				}
			} else {
				if (!quoted && (c == ' ' || c == ',')) {
					break;
				}
				if (strchr(allowed_alphabet, c) == NULL) {
					return 0;
				}
				if (!quoted && c == '$') {
					int id_end =
						find_id_end(str, end, i + 1);
					if (id_end < end &&
					    str[id_end] == '$') {
						return 0;
					}
					/* Still check every byte: '$' must not hide punctuation. */
				}
			}
			if (++name_len > 63) {
				return 0;
			}
			i++;
		}
		if (!closed || name_len == 0) {
			return 0;
		}
		if (!quoted) {
			int low = 0;
			int high = sizeof(reserved) / sizeof(reserved[0]);
			while (low < high) {
				int mid = low + (high - low) / 2;
				int cmp = strncasecmp(str + start,
						      reserved[mid], name_len);
				if (cmp == 0) {
					if (reserved[mid][name_len] == 0) {
						return 0;
					}
					cmp = -1;
				}
				if (cmp < 0) {
					high = mid;
				} else {
					low = mid + 1;
				}
			}
		}
		while (i < end && str[i] == ' ') {
			i++;
		}
		if (i == end) {
			return 1;
		}
		if (str[i++] != ',') {
			return 0;
		}
	}
}

static int kiwi_var_cas(kiwi_var_t *var, kiwi_var_t *server_var, char **query,
			int pos, int smart_enquoting_search_path,
			int *normalize_search_path)
{
	if (var->type == KIWI_VAR_UNDEF || kiwi_var_compare(var, server_var)) {
		return pos;
	}

	if (var->value_len <= 0) {
		return -1;
	}

	const char *prefix = "SET ";
	const char *suffix = ";";
	int name_len = var->name_len - 1;
	int search_path = var->type == KIWI_VAR_SEARCH_PATH;
	int raw_value = search_path &&
			kiwi_search_path_is_safe(var->value, var->value_len);
	if (search_path && smart_enquoting_search_path && !raw_value) {
		prefix = "SELECT pg_catalog.set_config('search_path',";
		suffix = ",false);";
		name_len = 0;
	}
	int prefix_len = strlen(prefix);
	int suffix_len = strlen(suffix);

	/* Each byte may need escaping; kiwi_enquote needs four spare bytes. */
	uint64_t size = (uint64_t)pos + prefix_len + name_len +
			(name_len != 0) + 2 * (uint64_t)(var->value_len - 1) +
			2 + 4 + suffix_len;
	/* kiwi_fe_write_query also adds a terminating NUL and Query header. */
	if (size > INT_MAX - sizeof(kiwi_header_t) - 1) {
		return -1;
	}
	char *buffer = realloc(*query, size);
	if (buffer == NULL) {
		return -1;
	}
	*query = buffer;

	memcpy(buffer + pos, prefix, prefix_len);
	pos += prefix_len;
	if (name_len != 0) {
		memcpy(buffer + pos, var->name, name_len);
		pos += name_len;
		buffer[pos++] = '=';
	}
	int quote_len;
	if (raw_value) {
		quote_len = var->value_len - 1;
		memcpy(buffer + pos, var->value, quote_len);
	} else {
		quote_len = kiwi_enquote(var->value, buffer + pos,
					 (int)size - pos - suffix_len);
	}
	if (quote_len == -1) {
		return -1;
	}
	pos += quote_len;
	memcpy(buffer + pos, suffix, suffix_len);
	pos += suffix_len;
	buffer[pos] = 0;
	if (search_path) {
		*normalize_search_path =
			raw_value || smart_enquoting_search_path;
	}
	return pos;
}

int kiwi_vars_cas(kiwi_vars_t *client, kiwi_vars_t *server, char **query,
		  int smart_enquoting_search_path, int *normalize_search_path)
{
	*query = NULL;
	*normalize_search_path = 0;
	int pos = 0;
	for (kiwi_var_type_t type = KIWI_VAR_CLIENT_ENCODING;
	     type < KIWI_VAR_MAX; type++) {
		if (type == KIWI_VAR_SEARCH_PATH ||
		    type == KIWI_VAR_COMPRESSION ||
		    type == KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS) {
			continue;
		}
		pos = kiwi_var_cas(kiwi_vars_of(client, type),
				   kiwi_vars_of(server, type), query, pos,
				   smart_enquoting_search_path,
				   normalize_search_path);
		if (pos == -1) {
			goto error;
		}
	}

	/* SELECT takes a snapshot: apply transaction settings before it. */
	pos = kiwi_var_cas(kiwi_vars_of(client, KIWI_VAR_SEARCH_PATH),
			   kiwi_vars_of(server, KIWI_VAR_SEARCH_PATH), query,
			   pos, smart_enquoting_search_path,
			   normalize_search_path);
	if (pos == -1) {
		goto error;
	}
	return pos;

error:
	free(*query);
	*query = NULL;
	return -1;
}

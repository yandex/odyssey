/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

#include <kiwi/kiwi.h>
#include <od_memory.h>
#include <sql/full/keywords.h>

#include <limits.h>

static int kiwi_search_path_is_safe(const char *str, int len)
{
	static const char *allowed_alphabet =
		"qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_0123456789,$ ";
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
		/* Inside an identifier, '$' cannot start a dollar-quoted string. */
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
			}
			if (++name_len >= NAMEDATALEN) {
				return 0;
			}
			i++;
		}
		if (!closed || name_len == 0) {
			return 0;
		}
		if (!quoted) {
			char name[NAMEDATALEN];
			memcpy(name, str + start, name_len);
			name[name_len] = 0;
			int keyword = ScanKeywordLookup(name, &ScanKeywords);
			if (keyword >= 0 && ScanKeywordCategories[keyword] ==
						    RESERVED_KEYWORD) {
				return 0;
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

static int kiwi_var_cas(kiwi_var_t *var, char **query, char *query_buf,
			int *query_capacity, int pos,
			int smart_enquoting_search_path,
			int *normalize_search_path)
{
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

	/* Reject oversized values before inspecting their contents. */
	uint64_t size = (uint64_t)pos + prefix_len + name_len +
			(name_len != 0) + 2 * (uint64_t)(var->value_len - 1) +
			2 + 4 + suffix_len;
	/* kiwi_fe_write_query also adds a terminating NUL and Query header. */
	if (size > INT_MAX - sizeof(kiwi_header_t) - 1) {
		return -1;
	}
	int value_len = var->value_len - 1;
	if (!raw_value) {
		value_len = 3; /* E'...' */
		for (const char *p = var->value; *p; p++) {
			value_len += 1 + (*p == '\'' || *p == '\\');
		}
	}
	size = (uint64_t)pos + prefix_len + name_len + (name_len != 0) +
	       value_len + suffix_len + 1;
	if (size > (uint64_t)*query_capacity) {
		char *buffer;
		if (*query == query_buf) {
			buffer = od_malloc(size);
			if (buffer != NULL && pos > 0) {
				memcpy(buffer, *query, pos);
			}
		} else {
			buffer = od_realloc(*query, size);
		}
		if (buffer == NULL) {
			return -1;
		}
		*query = buffer;
		*query_capacity = size;
	}
	char *buffer = *query;

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
		  char *query_buf, int query_buf_len,
		  int smart_enquoting_search_path, int *normalize_search_path)
{
	*query = query_buf;
	*normalize_search_path = 0;
	int pos = 0;
	for (kiwi_var_type_t type = KIWI_VAR_CLIENT_ENCODING;
	     type < KIWI_VAR_MAX; type++) {
		if (type == KIWI_VAR_SEARCH_PATH ||
		    type == KIWI_VAR_COMPRESSION ||
		    type == KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS) {
			continue;
		}
		kiwi_var_t *var = kiwi_vars_of(client, type);
		if (var->type == KIWI_VAR_UNDEF ||
		    kiwi_var_compare(var, kiwi_vars_of(server, type))) {
			continue;
		}
		pos = kiwi_var_cas(var, query, query_buf, &query_buf_len, pos,
				   smart_enquoting_search_path,
				   normalize_search_path);
		if (pos == -1) {
			goto error;
		}
	}

	/* SELECT takes a snapshot: apply transaction settings before it. */
	kiwi_var_t *var = kiwi_vars_of(client, KIWI_VAR_SEARCH_PATH);
	if (var->type != KIWI_VAR_UNDEF &&
	    !kiwi_var_compare(var,
			      kiwi_vars_of(server, KIWI_VAR_SEARCH_PATH))) {
		pos = kiwi_var_cas(var, query, query_buf, &query_buf_len, pos,
				   smart_enquoting_search_path,
				   normalize_search_path);
		if (pos == -1) {
			goto error;
		}
	}
	if (pos == 0) {
		*query = NULL;
	}
	return pos;

error:
	if (*query != query_buf) {
		od_free(*query);
	}
	*query = NULL;
	return -1;
}

static int kiwi_var_has_high_byte(kiwi_var_t *var)
{
	for (int i = 0; i < var->value_len; i++) {
		if ((unsigned char)var->value[i] >= 0x80) {
			return 1;
		}
	}
	return 0;
}

int kiwi_vars_cas_encoding(kiwi_vars_t *client, kiwi_vars_t *server, char *buf,
			   int buf_len)
{
	kiwi_var_t *enc = kiwi_vars_of(client, KIWI_VAR_CLIENT_ENCODING);
	if (enc->type == KIWI_VAR_UNDEF || enc->value_len <= 0 ||
	    kiwi_var_compare(enc,
			     kiwi_vars_of(server, KIWI_VAR_CLIENT_ENCODING))) {
		return 0;
	}

	int need = 0;
	for (kiwi_var_type_t type = KIWI_VAR_CLIENT_ENCODING + 1;
	     type < KIWI_VAR_MAX && !need; type++) {
		if (type == KIWI_VAR_COMPRESSION ||
		    type == KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS) {
			continue;
		}
		kiwi_var_t *var = kiwi_vars_of(client, type);
		if (var->type == KIWI_VAR_UNDEF ||
		    kiwi_var_compare(var, kiwi_vars_of(server, type))) {
			continue;
		}
		need = kiwi_var_has_high_byte(var);
	}
	if (!need) {
		return 0;
	}

	const char *prefix = "SET client_encoding=";
	int prefix_len = strlen(prefix);
	if (buf_len < prefix_len + 2) {
		return -1;
	}
	memcpy(buf, prefix, prefix_len);
	int quote_len =
		kiwi_enquote(enc->value, buf + prefix_len,
			     buf_len - prefix_len - 1 /* ';' */);
	if (quote_len == -1) {
		return -1;
	}
	int pos = prefix_len + quote_len;
	buf[pos++] = ';';
	buf[pos] = 0;
	return pos;
}

/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

#include <kiwi/kiwi.h>

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

static inline int is_safe_param(const char *str, int len)
{
	/*
	 * checks that the string contains only:
	 * - english letters
	 * - digits
	 * - _ and ,
	 * - double quotes
	 * - $, but not tags from dollar-enquoted strings
	 * - spaces
	 */

	static const char *allowed_alphabet =
		"qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_0123456789,\" ";

	for (int i = 0; i < len; ++i) {
		if (str[i] == '$') {
			/*
			 * it might be a dollar tag - forbidden entity
			 * need to skip id (possible empty) and check if it
			 * ends with $
			 * 
			 * ex:
			 * - $$
			 * - $a$
			 * - $_a123$
			 */

			++i; /* skip $ */

			int id_end = find_id_end(str, len, i + 1);
			if (id_end < len && str[id_end] == '$') {
				/* it is dollar tag, forbid */
				return 0;
			}

			if (id_end >= len) {
				/* $aaaa\0 situation - ok */
				return 1;
			}

			/* non-id symbol, need recheck that in allowed alphabet below */
			i = id_end;

			/* no continue */
		}

		if (strchr(allowed_alphabet, str[i]) == NULL) {
			return 0;
		}
	}

	return 1;
}

int kiwi_vars_cas(kiwi_vars_t *client, kiwi_vars_t *server, char *query,
		  int query_len, int smart_enquoting_search_path)
{
	int pos = 0;
	kiwi_var_type_t type;
	type = KIWI_VAR_CLIENT_ENCODING;
	for (; type < KIWI_VAR_MAX; type++) {
		kiwi_var_t *var;
		var = kiwi_vars_of(client, type);
		/* we do not support odyssey-to-backend compression yet */
		if (var->type == KIWI_VAR_UNDEF ||
		    var->type == KIWI_VAR_COMPRESSION ||
		    var->type ==
			    KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS /* never deploy this one */) {
			continue;
		}
		kiwi_var_t *server_var;
		server_var = kiwi_vars_of(server, type);
		if (kiwi_var_compare(var, server_var)) {
			continue;
		}

		/* SET key=quoted_value; */
		int size = 4 + (var->name_len - 1) + 1 + 1;
		if (query_len < size) {
			return -1;
		}
		memcpy(query + pos, "SET ", 4);
		pos += 4;
		memcpy(query + pos, var->name, var->name_len - 1);
		pos += var->name_len - 1;
		memcpy(query + pos, "=", 1);
		pos += 1;
		int quote_len;

		if (var->type != KIWI_VAR_SEARCH_PATH ||
		    !smart_enquoting_search_path ||
		    !is_safe_param(var->value, var->value_len)) {
			quote_len = kiwi_enquote(var->value, query + pos,
						 query_len - pos);
		} else {
			if (query_len - pos < var->value_len) {
				return -1;
			}
			quote_len = var->value_len - 1;
			memcpy(query + pos, var->value, var->value_len - 1);
		}
		if (quote_len == -1) {
			return -1;
		}
		pos += quote_len;
		memcpy(query + pos, ";", 1);
		pos += 1;
	}

	return pos;
}


/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

#include <kiwi/kiwi.h>

static inline void kiwi_long_option_rewrite(char *name, int name_len)
{
	assert(name);

	for (int i = 0; i < name_len; ++i) {
		if (name[i] == '-') {
			name[i] = '_';
		}
	}
}

static inline const char *pgopts_find_token_end(const char *s, const char *end)
{
	/* iterate forward until non-escaped space or end reached */

	int escape = 0;

	while (s < end && *s != '\0') {
		if (isspace(*s) && !escape) {
			break;
		}

		if (!escape && *s == '\\') {
			escape = 1;
		} else {
			escape = 0;
		}

		++s;
	}

	return s;
}

typedef struct {
	const char *ptr;
	size_t len;
} tok_info_t;

static inline tok_info_t pgopts_strtok(const char *str, const char *end,
				       const char **prev)
{
	/*
	 * keep pg_split_opts space escaping logic
	 * can't use strtok here because of that escaping
	 *
	 * interface is similar to strtok
	 */

	tok_info_t r;
	memset(&r, 0, sizeof(struct iovec));

	if (str == NULL) {
		str = *prev;
	}

	while (str < end && isspace(*str)) {
		++str;
	}

	if (str >= end || *str == '\0') {
		return r;
	}

	const char *e = pgopts_find_token_end(str, end);

	r.ptr = str;
	r.len = e - str;

	*prev = e + 1;

	return r;
}

static inline size_t find_eq_pos(const char *str, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (str[i] == '=') {
			return i;
		}
	}

	return len;
}

static inline size_t unescape(const char *str, size_t len, char *dst)
{
	int escape = 0;
	size_t rlen = 0;

	for (size_t i = 0; i < len; ++i) {
		if (!escape && str[i] == '\\') {
			escape = 1;
		} else {
			dst[rlen++] = str[i];
			escape = 0;
		}
	}

	dst[rlen] = 0;

	return rlen;
}

static inline int kiwi_parse_option_and_update_var(kiwi_vars_t *vars,
						   const char *str, size_t len)
{
	char name[KIWI_MAX_VAR_SIZE];
	char val[KIWI_MAX_VAR_SIZE];

	size_t equal_pos = find_eq_pos(str, len);
	if (equal_pos == len) {
		/* invalid token */
		return -1;
	}

	size_t nlen = equal_pos;
	size_t vlen = len - (equal_pos + 1);

	nlen = unescape(str, nlen, name);
	vlen = unescape(str + equal_pos + 1, vlen, val);

	kiwi_long_option_rewrite(name, (int)nlen);

	return kiwi_vars_update(vars, name, (int)nlen + 1, val, (int)vlen + 1);
}

int kiwi_parse_options_and_update_vars(kiwi_vars_t *vars, const char *str,
				       int slen)
{
	if (str == NULL) {
		return -1;
	}

	const char *end = str + slen;
	const char *prev = NULL;
	int next_read = 0;
	int rc = 0;

	tok_info_t tokk = pgopts_strtok(str, end, &prev);
	while (tokk.ptr != NULL) {
		const char *tok = tokk.ptr;
		size_t len = tokk.len;

		if (next_read) {
			/* must read var afrer -c now */
			next_read = 0;

			rc = kiwi_parse_option_and_update_var(vars, tok, len);
			if (rc != 0) {
				break;
			}
		} else if (len == 2 && tok[0] == '-' && tok[1] == 'c') {
			/*
			 * this is -c var=value
			 * must read value from next token
			 */
			next_read = 1;
		} else if (len > 2 && tok[0] == '-' && tok[1] == '-') {
			/* --var=value */
			tok += 2;
			len -= 2;
			rc = kiwi_parse_option_and_update_var(vars, tok, len);
			if (rc != 0) {
				break;
			}
		} else {
			/* unexpected token */
			return -1;
		}

		tokk = pgopts_strtok(NULL, end, &prev);
	}

	if (next_read) {
		/* expected key=value */
		return -1;
	}

	return rc;
}

#include <odyssey.h>

#include <od_memory.h>
#include <util.h>
#include <affinity.h>

void od_affinity_cpuset_init(od_affinity_cpuset_t *set)
{
	memset(set, 0, sizeof(od_affinity_cpuset_t));
}

int od_affinity_cpuset_add(od_affinity_cpuset_t *set, int i)
{
	if (i < 0 || i >= OD_AFFINITY_MAX_CPUS) {
		return -1;
	}

	int idx = i / (8 * sizeof(uint64_t));
	int bit = i % (8 * sizeof(uint64_t));
	set->bits[idx] |= ((uint64_t)1 << bit);

	return 0;
}

int od_affinity_cpuset_get(const od_affinity_cpuset_t *set, int i)
{
	if (i < 0 || i >= OD_AFFINITY_MAX_CPUS) {
		return -1;
	}

	int idx = i / (8 * sizeof(uint64_t));
	int bit = i % (8 * sizeof(uint64_t));

	return (set->bits[idx] & ((uint64_t)1 << bit)) != 0;
}

int od_affinity_cpuset_count(const od_affinity_cpuset_t *set)
{
	int count = 0;
	for (int i = 0; i < OD_AFFINITY_MAX_CPUS; ++i) {
		count += od_affinity_cpuset_get(set, i);
	}

	return count;
}

int od_affinity_cpuset_parse(const char *str, od_affinity_cpuset_t *set,
			     char *errbuf, size_t errbuf_len)
{
	od_affinity_cpuset_init(set);

	size_t len = strlen(str);
	const char *pos = str;
	const char *end = str + len;

	if (pos == end) {
		goto error;
	}

	errno = 0;

	while (pos < end) {
		if (!isdigit(*pos)) {
			goto error;
		}

		char *endp;
		long begin = strtol(pos, &endp, 10);
		int err_ = errno;
		if (err_ == EINVAL || err_ == ERANGE || begin < 0 ||
		    pos == endp) {
			goto error;
		}

		pos = endp;
		if (pos >= end) {
			/* single core at the end of the str */
			if (od_affinity_cpuset_add(set, begin) != 0) {
				goto error;
			}
			return 0;
		}

		if (*pos == ',') {
			/* begin,<smth else> */
			if (od_affinity_cpuset_add(set, begin) != 0) {
				goto error;
			}
			++pos;
			if (pos >= end) {
				/* trailing comma */
				goto error;
			}
			continue;
		}

		if (*pos == '-') {
			/* begin-end */
			++pos;
			if (pos >= end) {
				goto error;
			}

			if (!isdigit(*pos)) {
				goto error;
			}

			long ending = strtol(pos, &endp, 10);
			err_ = errno;
			if (err_ == EINVAL || err_ == ERANGE || begin < 0 ||
			    pos == endp) {
				goto error;
			}

			if (ending < begin) {
				goto error;
			}

			for (long i = begin; i <= ending; ++i) {
				if (od_affinity_cpuset_add(set, i) != 0) {
					goto error;
				}
			}

			pos = endp;

			if (pos >= end) {
				continue;
			}

			if (*pos == ',') {
				/*
				 * begin-ending,<smth else>
				 * skip comma
				 */
				++pos;
				if (pos >= end) {
					/* trailing comma */
					goto error;
				}

				continue;
			}

			goto error;
		}

		goto error;
	}

	return 0;

error:
	od_affinity_cpuset_init(set);
	if (errbuf != NULL) {
		od_snprintf(errbuf, errbuf_len, "can't convert '%s' to cpuset",
			    str);
	}
	return -1;
}

void od_affinity_cpuset_export(const od_affinity_cpuset_t *set,
			       cpu_set_t *cpuset)
{
	if (OD_AFFINITY_MAX_CPUS > CPU_SETSIZE) {
		abort();
	}

	CPU_ZERO(cpuset);

	for (int i = 0; i < OD_AFFINITY_MAX_CPUS; ++i) {
		int rc = od_affinity_cpuset_get(set, i);
		if (rc) {
			CPU_SET(i, cpuset);
		}
	}
}

void od_affinity_cpuset_to_str(const od_affinity_cpuset_t *set, char *out,
			       size_t max)
{
	int begin = -1;
	int prev = -1;
	char *pos = out;
	char *end = pos + max;

	for (int i = 0; i < OD_AFFINITY_MAX_CPUS; ++i) {
		int e = od_affinity_cpuset_get(set, i);
		if (!e) {
			if (begin != -1) {
				if (prev != begin) {
					pos += od_snprintf(pos, end - pos,
							   "%d-%d,", begin,
							   prev);
				} else {
					pos += od_snprintf(pos, end - pos,
							   "%d,", begin);
				}
			}

			begin = -1;
			prev = -1;
			continue;
		}

		if (begin == -1) {
			begin = i;
			prev = i;
			continue;
		}

		if (prev + 1 == i) {
			prev = i;
		} else {
			if (prev != begin) {
				pos += od_snprintf(pos, end - pos, "%d-%d,",
						   begin, prev);
			} else {
				pos += od_snprintf(pos, end - pos, "%d,",
						   begin);
			}

			begin = i;
			prev = i;
		}
	}

	if (begin != -1) {
		if (prev != begin) {
			pos += od_snprintf(pos, end - pos, "%d-%d", begin,
					   prev);
		} else {
			pos += od_snprintf(pos, end - pos, "%d", begin);
		}
	}
}

void od_affinity_rule_init(od_affinity_rule_t *rule)
{
	memset(rule, 0, sizeof(od_affinity_rule_t));
	rule->index = -1;
	rule->role = OD_AFFINITY_ROLE_NONE;
}

int od_affinity_rule_parse(const char *str, od_affinity_rule_t *rule,
			   char *errbuf, size_t errbuf_size)
{
	static const char *worker = "worker";
	static const char *handshake = "handshake";

	od_affinity_rule_init(rule);

	const char *pos = str;
	const char *end = str + strlen(str);

	/* read role name */
	while (pos < end && isalpha(*pos)) {
		++pos;
	}
	size_t rolelen = pos - str;
	if (rolelen == strlen(worker) && strncmp(str, worker, rolelen) == 0) {
		rule->role = OD_AFFINITY_ROLE_WORKER;
	} else if (rolelen == strlen(handshake) &&
		   strncmp(str, handshake, rolelen) == 0) {
		rule->role = OD_AFFINITY_ROLE_HANDSHAKE;
	} else {
		goto error;
	}

	/* read possible index */
	if (pos < end && *pos == '[') {
		++pos;
		if (pos >= end) {
			goto error;
		}

		char *endp;
		errno = 0;
		long index = strtol(pos, &endp, 10);
		int err_ = errno;
		if (err_ == EINVAL || err_ == ERANGE || pos == endp ||
		    index < 0 || index > INT32_MAX) {
			goto error;
		}

		pos = endp;

		rule->index = (int)index;

		/* check ']' */
		if (pos >= end) {
			goto error;
		}

		if (*pos != ']') {
			goto error;
		}

		++pos;
	}

	/* must read ':' */
	if (pos >= end) {
		goto error;
	}

	if (*pos != ':') {
		goto error;
	}

	++pos;

	/* read cpuset */
	int rc = od_affinity_cpuset_parse(pos, &rule->cpuset, errbuf,
					  errbuf_size);
	if (rc != 0) {
		goto error_ret;
	}

	return 0;

error:
	if (errbuf != NULL) {
		od_snprintf(errbuf, errbuf_size,
			    "can't parse affinity rule '%s'", str);
	}

error_ret:
	od_affinity_rule_init(rule);
	return -1;
}

void od_affinity_config_init(od_affinity_config_t *config)
{
	memset(config, 0, sizeof(od_affinity_config_t));
	for (size_t i = 0; i < OD_AFFINITY_MAX_RULES; ++i) {
		od_affinity_rule_init(&config->rules[i]);
	}
}

static int has_rule(const od_affinity_config_t *config,
		    const od_affinity_rule_t *rule)
{
	for (size_t i = 0; i < config->nrules; ++i) {
		const od_affinity_rule_t *r = &config->rules[i];

		if (rule->role == r->role && rule->index == r->index) {
			return 1;
		}
	}

	return 0;
}

static int parse_impl(char *str, od_affinity_config_t *config, char *errbuf,
		      size_t errbuf_len)
{
	if (str == NULL) {
		if (errbuf != NULL) {
			od_snprintf(errbuf, errbuf_len,
				    "affinity config is null");
		}
		return -1;
	}

	od_affinity_config_init(config);

	if (strcmp(str, "off") == 0 || strcmp(str, "no") == 0) {
		config->mode = OD_AFFINITY_MODE_OFF;
		return 0;
	}

	if (strcmp(str, "auto") == 0 || strcmp(str, "yes") == 0) {
		config->mode = OD_AFFINITY_MODE_AUTO;
		return 0;
	}

	config->mode = OD_AFFINITY_MODE_RULES;

	static const char *spaces = " \t";
	char *save = NULL;
	char *rule_tok = strtok_r(str, spaces, &save);
	while (rule_tok != NULL) {
		if (config->nrules == OD_AFFINITY_MAX_RULES) {
			if (errbuf != NULL) {
				od_snprintf(
					errbuf, errbuf_len,
					"can't parse affinity config: too many rules");
			}

			goto error;
		}

		od_affinity_rule_t rule;
		int rc = od_affinity_rule_parse(rule_tok, &rule, errbuf,
						errbuf_len);
		if (rc < 0) {
			goto error;
		}

		if (has_rule(config, &rule)) {
			if (errbuf != NULL) {
				od_snprintf(errbuf, errbuf_len,
					    "rule '%s' duplicated", rule_tok);
			}

			goto error;
		}

		memcpy(&config->rules[config->nrules++], &rule,
		       sizeof(od_affinity_rule_t));

		rule_tok = strtok_r(NULL, spaces, &save);
	}

	if (config->nrules == 0) {
		if (errbuf != NULL) {
			od_snprintf(errbuf, errbuf_len,
				    "can't parse affinity config: empty rules");
		}
		goto error;
	}

	return 0;

error:
	od_affinity_config_init(config);
	return -1;
}

int od_affinity_config_parse(const char *str, size_t len,
			     od_affinity_config_t *config, char *errbuf,
			     size_t errbuf_len)
{
	char *copy = od_strndup(str, len);
	if (copy == NULL) {
		od_snprintf(errbuf, errbuf_len, "out of memory");
		return -1;
	}

	int rc = parse_impl(copy, config, errbuf, errbuf_len);

	od_free(copy);

	return rc;
}

od_affinity_mode_t od_affinity_resolve(const od_affinity_config_t *config,
				       od_affinity_rule_t *out,
				       od_affinity_role_t role, int idx)
{
	const od_affinity_rule_t *by_role_and_idx = NULL;
	const od_affinity_rule_t *by_role = NULL;

	if (config->mode == OD_AFFINITY_MODE_OFF) {
		return OD_AFFINITY_MODE_OFF;
	}

	if (config->mode == OD_AFFINITY_MODE_AUTO) {
		return OD_AFFINITY_MODE_AUTO;
	}

	for (size_t i = 0; i < config->nrules; ++i) {
		const od_affinity_rule_t *rule = &config->rules[i];

		if (rule->role == role && rule->index == idx) {
			by_role_and_idx = rule;
			break;
		}

		if (rule->role == role && rule->index == -1) {
			by_role = rule;
		}
	}

	if (by_role_and_idx != NULL) {
		memcpy(out, by_role_and_idx, sizeof(od_affinity_rule_t));
		return OD_AFFINITY_MODE_RULES;
	}

	if (by_role != NULL) {
		memcpy(out, by_role, sizeof(od_affinity_rule_t));
		return OD_AFFINITY_MODE_RULES;
	}

	return OD_AFFINITY_MODE_OFF;
}

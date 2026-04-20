#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdint.h>
#include <stddef.h>

#include <sched.h>

#include <misc.h>

#ifndef OD_AFFINITY_MAX_CPUS
#define OD_AFFINITY_MAX_CPUS CPU_SETSIZE
#endif

#ifndef OD_AFFINITY_MAX_RULES
#define OD_AFFINITY_MAX_RULES 128
#endif

typedef enum {
	OD_AFFINITY_MODE_OFF = 0,
	OD_AFFINITY_MODE_AUTO,
	OD_AFFINITY_MODE_RULES
} od_affinity_mode_t;

typedef struct {
	uint64_t bits[ROUNDUPDIV(OD_AFFINITY_MAX_CPUS, 8 * sizeof(uint64_t))];
} od_affinity_cpuset_t;

void od_affinity_cpuset_init(od_affinity_cpuset_t *set);
int od_affinity_cpuset_add(od_affinity_cpuset_t *set, int i);
int od_affinity_cpuset_get(const od_affinity_cpuset_t *set, int i);
int od_affinity_cpuset_count(const od_affinity_cpuset_t *set);
/*
 * convert internal mask representation to cpu_set_t suitable for
 * pthread_setaffinity_np() / sched_setaffinity().
 */
void od_affinity_cpuset_export(const od_affinity_cpuset_t *set,
			       cpu_set_t *cpuset);
void od_affinity_cpuset_to_str(const od_affinity_cpuset_t *set, char *out,
			       size_t max);

/*
 * parse a cpuset string:
 *   "0"
 *   "0,1,2"
 *   "0-3"
 *   "0-3,8-11"
 */
int od_affinity_cpuset_parse(const char *str, od_affinity_cpuset_t *mask,
			     char *errbuf, size_t errbuf_len);

typedef enum {
	OD_AFFINITY_ROLE_NONE = 0,
	OD_AFFINITY_ROLE_WORKER,
	OD_AFFINITY_ROLE_HANDSHAKE,
} od_affinity_role_t;

typedef struct {
	od_affinity_cpuset_t cpuset;
	od_affinity_role_t role;
	/* -1 if worker/handshake id is not specified */
	int index;
} od_affinity_rule_t;

void od_affinity_rule_init(od_affinity_rule_t *rule);
/*
 * parse a rule string:
 *   "worker:0"
 *   "handshake[1]:2"
 *   "handshake:0-3"
 *   "worker:0-3,8-11"
 */
int od_affinity_rule_parse(const char *str, od_affinity_rule_t *rule,
			   char *errbuf, size_t errbuf_size);

typedef struct {
	od_affinity_mode_t mode;
	od_affinity_rule_t rules[OD_AFFINITY_MAX_RULES];
	size_t nrules;
} od_affinity_config_t;

void od_affinity_config_init(od_affinity_config_t *config);
int od_affinity_config_parse(const char *str, size_t len,
			     od_affinity_config_t *config, char *errbuf,
			     size_t errbuf_len);
od_affinity_mode_t od_affinity_resolve(const od_affinity_config_t *config,
				       od_affinity_rule_t *out,
				       od_affinity_role_t role, int idx);

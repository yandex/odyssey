#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium.h>
#include <odyssey.h>

struct od_soft_oom_checker {
	od_config_soft_oom_t *config;
	int64_t machine_id;
	machine_wait_flag_t *stop_flag;

	atomic_uint_fast64_t current_memory_usage;
};

/* starts at separated thread because it uses file i/o */
int od_soft_oom_start_checker(od_config_soft_oom_t *config,
			      od_soft_oom_checker_t *checker);

void od_soft_oom_stop_checker(od_soft_oom_checker_t *checker);

int od_soft_oom_is_in_soft_oom(od_soft_oom_checker_t *checker,
			       uint64_t *used_memory);

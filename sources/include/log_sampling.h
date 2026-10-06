#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

static inline int od_log_sampling_hit(int sampling_percent)
{
	if (sampling_percent >= 100) {
		return 1;
	}
	if (sampling_percent <= 0) {
		return 0;
	}

	return (machine_lrand48() % 100) < sampling_percent;
}

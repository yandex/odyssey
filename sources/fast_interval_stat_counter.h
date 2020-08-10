#ifndef ODYSSEY_FAST_INTERVAL_STAT_CNTR_H
#define ODYSSEY_FAST_INTERVAL_STAT_CNTR_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <sys/time.h>
#include "macro.h"
#include <stddef.h>

typedef struct od_fast_interval_cntr od_fast_interval_cntr_t;

struct od_fast_interval_cntr
{
	int last_stat_switch_sec;
	int interval;
	int current_interval_cnt;
	int prev_interval_cnt;
};

void
od_fast_interval_cntr_init(od_fast_interval_cntr_t *c, int interval);

void
od_fast_interval_cntr_store(od_fast_interval_cntr_t *c, int add);

int
od_fast_interval_cntr_get_aggr(od_fast_interval_cntr_t *c);

#endif /* ODYSSEY_FAST_INTERVAL_STAT_CNTR_H */

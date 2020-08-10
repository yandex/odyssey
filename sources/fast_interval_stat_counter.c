
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "fast_interval_stat_counter.h"

void
od_fast_interval_cntr_init(od_fast_interval_cntr_t *c, int interval)
{
	c->prev_interval_cnt    = 0;
	c->current_interval_cnt = 0;
	c->interval             = interval;
	return;
}

void
od_fast_interval_cntr_store(od_fast_interval_cntr_t *c, int add)
{

	struct timeval tv;
	gettimeofday(&tv, NULL);

	if (c->last_stat_switch_sec + c->interval <= tv.tv_sec) {
		c->prev_interval_cnt    = c->current_interval_cnt;
		c->current_interval_cnt = 0;
		c->last_stat_switch_sec = tv.tv_sec;
	}

	c->current_interval_cnt += add;
}

int
od_fast_interval_cntr_get_aggr(od_fast_interval_cntr_t *c)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	if (c->last_stat_switch_sec + c->interval <= tv.tv_sec) {
		return c->current_interval_cnt;
	}

	return c->prev_interval_cnt;
}

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

struct od_cron {
	uint64_t stat_time_us;
	od_global_t *global;
	od_atomic_u64_t startup_errors;

#ifdef PROM_FOUND
	od_prom_metrics_t *metrics;
#endif

	machine_wait_flag_t *can_be_freed;
	atomic_int online;
};

od_retcode_t od_cron_init(od_cron_t *);
int od_cron_start(od_cron_t *, od_global_t *);
od_retcode_t od_cron_stop_and_wait(od_cron_t *cron);

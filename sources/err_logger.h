#ifndef ODYSSEY_ERR_LOGGER_H
#define ODYSSEY_ERR_LOGGER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "counter.h"

/* 300 = 5 * 60 */
#define DEFAULT_ERROR_INTERVAL_NUMBER 300

typedef struct od_error_logger od_error_logger_t;

struct od_error_logger
{
	size_t intercals_cnt;

	pthread_mutex_t lock;

	size_t current_interval_num;
	// var len
	od_counter_t *interval_counters[0];
};

extern od_retcode_t
od_error_logger_store_err(od_error_logger_t *l, size_t err_t);

extern od_error_logger_t *
od_err_logger_create(size_t intervals_count);

static inline od_error_logger_t *
od_err_logger_create_default()
{
	return od_err_logger_create(DEFAULT_ERROR_INTERVAL_NUMBER);
}

od_retcode_t
od_err_logger_free(od_error_logger_t *err_logger);

static inline od_retcode_t
od_err_logger_inc_interval(od_error_logger_t *l)
{
	pthread_mutex_lock(&l->lock);
	{
		++l->current_interval_num;
		l->current_interval_num %= l->intercals_cnt;

		od_counter_reset_all(l->interval_counters[l->current_interval_num]);
	}
	pthread_mutex_unlock(&l->lock);

	return OK_RESPONSE;
}

static inline size_t
od_err_logger_get_aggr_errors_count(od_error_logger_t *l, size_t err_t)
{
	size_t ret_val = 0;
	for (size_t i = 0; i < l->intercals_cnt; ++i) {
		ret_val += od_counter_get_count(l->interval_counters[i], err_t);
	}
	return ret_val;
}

#endif // ODYSSEY_ERR_LOGGER_H

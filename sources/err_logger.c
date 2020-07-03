
#include "err_logger.h"

od_error_logger_t *
od_err_logger_create(size_t intervals_count)
{
	od_error_logger_t *l = malloc(sizeof(od_error_logger_t));

	l->intercals_cnt        = intervals_count;
	l->current_interval_num = 0;

	l->interval_counters = malloc(sizeof(od_counter_t) * intervals_count);

	for (size_t i = 0; i < intervals_count; ++i) {
		l->interval_counters[i] = od_counter_create_default();
	}

	pthread_mutex_init(&l->lock, NULL);

	return l;
}

od_retcode_t
od_err_logger_free(od_error_logger_t *l)
{
	for (size_t i = 0; i < l->intercals_cnt; ++i) {
		int rc = od_counter_free(l->interval_counters[i]);
		if (rc != OK_RESPONSE)
			return rc;
	}

	pthread_mutex_destroy(&l->lock);
	free(l);

	return OK_RESPONSE;
}

od_retcode_t
od_error_logger_store_err(od_error_logger_t *l, size_t err_t)
{
	od_counter_inc(l->interval_counters[l->current_interval_num], err_t);
	return OK_RESPONSE;
}

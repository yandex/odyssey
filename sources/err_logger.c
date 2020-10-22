
#include "err_logger.h"

od_error_logger_t *
od_err_logger_create(size_t intervals_count)
{
	od_error_logger_t *err_logger = malloc(sizeof(od_error_logger_t));
	if (err_logger == NULL) {
		return NULL;
	}

	err_logger->intercals_cnt        = intervals_count;
	err_logger->current_interval_num = 0;

	err_logger->interval_counters =
	  malloc(sizeof(od_counter_t) * intervals_count);

	for (size_t i = 0; i < intervals_count; ++i) {
		err_logger->interval_counters[i] = od_counter_create_default();
		if (err_logger->interval_counters[i] == NULL) {
			free(err_logger->interval_counters);
			free(err_logger);
			return NULL;
		}
	}

	pthread_mutex_init(&err_logger->lock, NULL);

	return err_logger;
}

od_retcode_t
od_err_logger_free(od_error_logger_t *err_logger)
{
	for (size_t i = 0; i < err_logger->intercals_cnt; ++i) {
		int rc = od_counter_free(err_logger->interval_counters[i]);
		if (rc != OK_RESPONSE)
			return rc;
	}

	pthread_mutex_destroy(&err_logger->lock);
	free(err_logger);

	return OK_RESPONSE;
}

od_retcode_t
od_error_logger_store_err(od_error_logger_t *l, size_t err_t)
{
	od_counter_inc(l->interval_counters[l->current_interval_num], err_t);
	return OK_RESPONSE;
}

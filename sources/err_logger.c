
#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

static size_t err_logger_required_buf_size(size_t sz)
{
	return sizeof(od_error_logger_t) + (sz * sizeof(od_counter_t *));
}

#if defined(__x86_64__) || defined(__i386) || defined(_X86_)
#define OD_ERR_LOGGER_CAS_BACKOFF __asm__("pause")
#else
#define OD_ERR_LOGGER_CAS_BACKOFF
#endif

od_error_logger_t *od_err_logger_create(size_t intervals_count)
{
	od_error_logger_t *err_logger =
		od_malloc(err_logger_required_buf_size(intervals_count));
	if (err_logger == NULL) {
		goto error;
	}

	err_logger->intervals_cnt = intervals_count;
	atomic_init(&err_logger->current_interval_num, 0);

	for (size_t i = 0; i < intervals_count; ++i) {
		// used for router and frontend statuses
		// so, let it be 100
		err_logger->interval_counters[i] = od_counter_create(100UL);
		if (err_logger->interval_counters[i] == NULL) {
			goto error;
		}
	}

	return err_logger;
error:

	if (err_logger) {
		for (size_t i = 0; i < err_logger->intervals_cnt; ++i) {
			if (err_logger->interval_counters[i] == NULL)
				continue;
			od_counter_free(err_logger->interval_counters[i]);
		}

<<<<<<< HEAD
		pthread_mutex_destroy(&err_logger->lock);
		od_free((void *)(err_logger));
=======
		free((void *)(err_logger));
>>>>>>> e7e33917 (fix data race in err_logger)
	}

	return NULL;
}

od_retcode_t od_err_logger_free(od_error_logger_t *err_logger)
{
	if (err_logger == NULL) {
		return OK_RESPONSE;
	}

	for (size_t i = 0; i < err_logger->intervals_cnt; ++i) {
		if (err_logger->interval_counters[i] == NULL) {
			continue;
		}
		int rc = od_counter_free(err_logger->interval_counters[i]);
		err_logger->interval_counters[i] = NULL;

		if (rc != OK_RESPONSE)
			return rc;
	}

	od_free((void *)(err_logger));
	
	return OK_RESPONSE;
}

od_retcode_t od_error_logger_store_err(od_error_logger_t *l, size_t err_t)
{
	od_counter_inc(l->interval_counters[l->current_interval_num], err_t);
	return OK_RESPONSE;
}

od_retcode_t od_err_logger_inc_interval(od_error_logger_t *l)
{
	size_t old = atomic_load(&l->current_interval_num);
	while (!atomic_compare_exchange_weak(&l->current_interval_num, &old,
					     (old + 1) % l->intervals_cnt)) {
		OD_ERR_LOGGER_CAS_BACKOFF;
	}

	size_t new = (old + 1) % l->intervals_cnt;
	// some errors may be lost
	od_counter_reset_all(l->interval_counters[new]);

	return OK_RESPONSE;
}

size_t od_err_logger_get_aggr_errors_count(od_error_logger_t *l, size_t err_t)
{
	size_t ret_val = 0;
	for (size_t i = 0; i < l->intervals_cnt; ++i) {
		ret_val += od_counter_get_count(l->interval_counters[i], err_t);
	}
	return ret_val;
}


/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <ejection.h>
#include <od_memory.h>
#include <config.h>

static inline int queue_inc_ptr(od_conn_eject_info *info, int ptr)
{
	++ptr;
	if (ptr >= info->limit) {
		ptr = 0;
	}

	return ptr;
}

static inline void queue_pop(od_conn_eject_info *info)
{
	if (info->size == 0) {
		return;
	}

	info->head = queue_inc_ptr(info, info->head);
	--info->size;
}

static inline int queue_push(od_conn_eject_info *info, uint64_t now_ms)
{
	if (info->size == info->limit) {
		return 0;
	}

	info->queue[info->tail] = now_ms;
	info->tail = queue_inc_ptr(info, info->tail);
	++info->size;

	return 1;
}

static inline void queue_clean(od_conn_eject_info *info, uint64_t now_ms)
{
	while (info->size > 0) {
		if (info->queue[info->head] + (uint64_t)info->interval_ms >
		    now_ms) {
			break;
		}

		queue_pop(info);
	}
}

od_retcode_t od_conn_eject_info_init(od_conn_eject_info **info,
				     const od_config_conn_drop_options_t *opts)
{
	*info = (od_conn_eject_info *)od_malloc(sizeof(od_conn_eject_info));
	if (*info == NULL) {
		/* TODO: set errno properly */

		return NOT_OK_RESPONSE;
	}
	memset(*info, 0, sizeof(od_conn_eject_info));

	pthread_spin_init(&(*info)->mu, PTHREAD_PROCESS_PRIVATE);

	(*info)->head = 0;
	(*info)->tail = 0;
	(*info)->size = 0;
	(*info)->interval_ms = opts->interval_ms;
	(*info)->limit = opts->rate;
	if ((*info)->limit <= 0) {
		return OK_RESPONSE;
	}

	(*info)->queue = od_malloc((*info)->limit * sizeof(uint64_t));
	if ((*info)->queue == NULL) {
		od_free(*info);
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

od_retcode_t od_conn_eject_info_free(od_conn_eject_info *info)
{
	pthread_spin_destroy(&info->mu);

	od_free(info->queue);
	od_free(info);

	return OK_RESPONSE;
}

int od_conn_eject_info_try(od_conn_eject_info *info, uint64_t now_ms)
{
	int rc = 0;

	pthread_spin_lock(&info->mu);

	if (info->limit > 0) {
		queue_clean(info, now_ms);
		rc = queue_push(info, now_ms);
	} else {
		rc = 1;
	}

	pthread_spin_unlock(&info->mu);

	return rc;
}

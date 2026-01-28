#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/* only for online restart */

#include <pthread.h>

#include <types.h>

typedef struct {
	/* TODO: eject info is thread private, so mu is useless? */
	pthread_spinlock_t mu;
	int interval_ms;
	int limit;
	uint64_t *queue;
	int head;
	int tail;
	int size;
} od_conn_eject_info;

extern od_retcode_t
od_conn_eject_info_init(od_conn_eject_info **dst,
			const od_config_conn_drop_options_t *opts);
extern od_retcode_t od_conn_eject_info_free(od_conn_eject_info *ptr);

int od_conn_eject_info_try(od_conn_eject_info *info, uint64_t now_ms);

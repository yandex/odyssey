//
// Created by ein-krebs on 8/12/21.
//

#ifndef ODYSSEY_PROM_METRICS_H
#define ODYSSEY_PROM_METRICS_H
#include <prom.h>

typedef struct od_prom_metrics od_prom_metrics_t;

struct od_prom_metrics {
	prom_gauge_t *msg_allocated;
	prom_gauge_t *msg_cache_count;
	prom_gauge_t *msg_cache_gc_count;
	prom_gauge_t *msg_cache_size;
	prom_gauge_t *count_coroutine;
	prom_gauge_t *count_coroutine_cache;

	prom_gauge_t *database_len;
	prom_gauge_t *user_len;
	prom_gauge_t *client_pool_total;
	prom_gauge_t *server_pool_active;
	prom_gauge_t *server_pool_idle;
	prom_gauge_t *avg_tx_count;
	prom_gauge_t *avg_tx_time;
	prom_gauge_t *avg_query_count;
	prom_gauge_t *avg_query_time;
	prom_gauge_t *avg_recv_client;
	prom_gauge_t *avg_recv_server;
};

extern int od_prom_metrics_init(od_prom_metrics_t *self);

extern int od_prom_metrics_write_stat(od_prom_metrics_t *self,
				      u_int64_t msg_allocated,
				      u_int64_t msg_cache_count,
				      u_int64_t msg_cache_gc_count,
				      u_int64_t msg_cache_size,
				      u_int64_t count_coroutine,
				      u_int64_t count_coroutine_cache);

extern const char *od_prom_metrics_get_stat();

extern void od_prom_free(void *__ptr);

#endif //ODYSSEY_PROM_METRICS_H

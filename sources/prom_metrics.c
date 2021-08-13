//
// Created by ein-krebs on 8/12/21.
//

#include <prom_metrics.h>
#include <prom.h>
#include <assert.h>

int od_prom_metrics_init(struct od_prom_metrics *self)
{
	if (self == NULL)
		return 0;
	int err = prom_collector_registry_default_init();
	if (err)
		return 1;
	self->msg_allocated = prom_collector_registry_must_register_metric(
		prom_gauge_new("msg_allocated", "Messages allocated", 0, NULL));
	self->msg_cache_count = prom_collector_registry_must_register_metric(
		prom_gauge_new("msg_cache_count", "Messages cached", 0, NULL));
	self->msg_cache_gc_count =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"msg_cache_gc_count", "Messages freed", 0, NULL));
	self->msg_cache_size =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"msg_cache_size", "Messages cache size", 0, NULL));
	self->count_coroutine =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"count_coroutine", "Coroutines running", 0, NULL));
	self->count_coroutine_cache =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"count_coroutine_cache", "Coroutines cached", 0, NULL));
	self->database_len =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"database_len", "Total databases count", 0, NULL));
	self->user_len =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"user_len", "Total users count", 0, NULL));
	const char **database_labels = ["database"];
	const char **user_database_labels = ["user", "database"];
	self->client_pool_total =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"client_pool_total", "Total clients count", 1, database_labels));
	self->server_pool_active =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"server_pool_active", "Active servers count", 0, NULL));
	self->server_pool_idle =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"sever_pool_idle", "Idle servers count", 0, NULL));
	self->avg_tx_count =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"avg_tx_count", "Average transactions count per second", 1, user_database_labels));
	self->avg_tx_time =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"avg_tx_time", "Average transaction time in usec", 1, user_database_labels));
	self->avg_query_count =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"avg_query_count", "Average query count per second", 1, user_database_labels));
	self->avg_query_time =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"avg_query_time", "Average query time in usec", 1, user_database_labels));
	self->avg_recv_client =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"avg_recv_client", "Average in bytes/sec", 1, user_database_labels));
	self->avg_recv_server =
		prom_collector_registry_must_register_metric(prom_gauge_new(
			"avg_recv_server", "Average out bytes/sec", 1, user_database_labels));
	return 0;
}

int od_prom_metrics_write_logs(struct od_prom_metrics *self,
			       u_int64_t msg_allocated,
			       u_int64_t msg_cache_count,
			       u_int64_t msg_cache_gc_count,
			       u_int64_t msg_cache_size,
			       u_int64_t count_coroutine,
			       u_int64_t count_coroutine_cache)
{
	if (self == NULL)
		return 1;
	int err;
	err = prom_gauge_set(self->msg_allocated, (double)msg_allocated, NULL);
	if (err)
		return 2;
	err = prom_gauge_set(self->msg_cache_count, (double)msg_cache_count,
			     NULL);
	if (err)
		return 2;
	err = prom_gauge_set(self->msg_cache_gc_count,
			     (double)msg_cache_gc_count, NULL);
	if (err)
		return 2;
	err = prom_gauge_set(self->msg_cache_size, (double)msg_cache_size,
			     NULL);
	if (err)
		return 2;
	err = prom_gauge_set(self->count_coroutine, (double)count_coroutine,
			     NULL);
	if (err)
		return 2;
	err = prom_gauge_set(self->count_coroutine_cache,
			     (double)count_coroutine_cache, NULL);
	if (err)
		return 2;
	return 0;
}

const char *od_prom_metrics_get_logs()
{
	return prom_collector_registry_bridge(PROM_COLLECTOR_REGISTRY_DEFAULT);
}

void od_prom_free(void *__ptr)
{
	prom_free(__ptr);
}

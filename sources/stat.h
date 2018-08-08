#ifndef OD_STAT_H
#define OD_STAT_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_stat od_stat_t;

struct od_stat
{
	od_atomic_u64_t count_query;
	od_atomic_u64_t count_tx;
	od_atomic_u64_t query_time;
	uint64_t        query_time_start;
	od_atomic_u64_t tx_time;
	uint64_t        tx_time_start;
	od_atomic_u64_t recv_server;
	od_atomic_u64_t recv_client;
	od_atomic_u64_t count_error;
};

static inline void
od_stat_init(od_stat_t *stat)
{
	memset(stat, 0, sizeof(*stat));
}

static inline void
od_stat_query_start(od_stat_t *stat)
{
	if (! stat->query_time_start)
		stat->query_time_start = machine_time();

	if (! stat->tx_time_start)
		stat->tx_time_start = machine_time();
}

static inline void
od_stat_query_end(od_stat_t *stat, int in_transaction, int64_t *query_time)
{
	int64_t diff;
	if (stat->query_time_start) {
		diff = machine_time() - stat->query_time_start;
		if (diff > 0) {
			*query_time = diff;
			od_atomic_u64_add(&stat->query_time, diff);
			od_atomic_u64_inc(&stat->count_query);
		}
		stat->query_time_start = 0;
	}

	if (in_transaction)
		return;

	if (stat->tx_time_start) {
		diff = machine_time() - stat->tx_time_start;
		if (diff > 0) {
			od_atomic_u64_add(&stat->tx_time, diff);
			od_atomic_u64_inc(&stat->count_tx);
		}
		stat->tx_time_start = 0;
	}
}

static inline void
od_stat_recv_server(od_stat_t *stat, uint64_t bytes)
{
	od_atomic_u64_add(&stat->recv_server, bytes);
}

static inline void
od_stat_recv_client(od_stat_t *stat, uint64_t bytes)
{
	od_atomic_u64_add(&stat->recv_client, bytes);
}

static inline void
od_stat_error(od_stat_t *stat)
{
	od_atomic_u64_inc(&stat->count_error);
}

static inline void
od_stat_sum(od_stat_t *sum, od_stat_t *stat)
{
	sum->count_query += od_atomic_u64_of(&stat->count_query);
	sum->count_tx    += od_atomic_u64_of(&stat->count_tx);
	sum->query_time  += od_atomic_u64_of(&stat->query_time);
	sum->tx_time     += od_atomic_u64_of(&stat->tx_time);
	sum->recv_client += od_atomic_u64_of(&stat->recv_client);
	sum->recv_server += od_atomic_u64_of(&stat->recv_server);
}

static inline void
od_stat_average(od_stat_t *avg, od_stat_t *current, od_stat_t *prev,
                uint64_t prev_time_us)
{
	const uint64_t interval_usec = 1000000;
	uint64_t interval_us;
	interval_us = machine_time() - prev_time_us;
	if (interval_us <= 0)
		return;

	uint64_t count_query;
	uint64_t count_tx;
	count_query = current->count_query - prev->count_query;
	count_tx    = current->count_tx - prev->count_tx;

	avg->count_query = (count_query * interval_usec) / interval_us;
	avg->count_tx    = (count_tx * interval_usec) / interval_us;

	if (count_query > 0)
		avg->query_time = (current->query_time - prev->query_time) / count_query;
	if (count_tx > 0)
		avg->tx_time = (current->tx_time - prev->tx_time) / count_tx;

	avg->recv_client = ((current->recv_client - prev->recv_client) * interval_usec) /
	                    interval_us;
	avg->recv_server = ((current->recv_server - prev->recv_server) * interval_usec) /
	                    interval_us;
}

#endif /* OD_STAT_H */

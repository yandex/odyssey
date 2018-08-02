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
	od_atomic_u64_t count_request;
	od_atomic_u64_t count_reply;
	od_atomic_u64_t count_query;
	od_atomic_u64_t count_tx;
	od_atomic_u64_t query_time;
	uint64_t        query_time_start;
	od_atomic_u64_t tx_time;
	uint64_t        tx_time_start;
	od_atomic_u64_t recv_server;
	od_atomic_u64_t recv_client;
	uint64_t        count_error;
};

static inline void
od_stat_init(od_stat_t *stat)
{
	memset(stat, 0, sizeof(*stat));
}

static inline int
od_stat_sync_is(od_stat_t *stat)
{
	return stat->count_request == stat->count_reply;
}

static inline void
od_stat_sync_request(od_stat_t *stat, uint64_t count)
{
	od_atomic_u64_add(&stat->count_request, count);
}

static inline void
od_stat_sync_reply(od_stat_t *stat)
{
	od_atomic_u64_inc(&stat->count_reply);
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
	stat->count_error++;
}

#endif /* OD_STAT_H */

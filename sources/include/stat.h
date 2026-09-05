#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdatomic.h>
#include <tdigest.h>

#define QUANTILES_WINDOW 2
#define QUANTILES_COMPRESSION 100

typedef struct od_stat_state od_stat_state_t;
typedef struct od_stat od_stat_t;

struct od_stat_state {
	uint64_t query_time_start;
	uint64_t tx_time_start;
};

struct od_stat {
	bool enable_quantiles;
	uint8_t current_tdigest;

	atomic_uint_fast64_t count_query;
	atomic_uint_fast64_t count_tx;

	atomic_uint_fast64_t query_time;
	atomic_uint_fast64_t tx_time;

	atomic_uint_fast64_t count_wait;
	atomic_uint_fast64_t wait_time_us;

	atomic_uint_fast64_t recv_server;
	atomic_uint_fast64_t recv_client;
	atomic_uint_fast64_t count_parse;
	atomic_uint_fast64_t count_parse_reuse;
	atomic_uint_fast64_t count_cancel;

	td_histogram_t *transaction_hgram[QUANTILES_WINDOW];
	td_histogram_t *query_hgram[QUANTILES_WINDOW];
};

static inline void od_stat_state_init(od_stat_state_t *state)
{
	memset(state, 0, sizeof(*state));
}

static inline void od_stat_init(od_stat_t *stat)
{
	memset(stat, 0, sizeof(*stat));
}

static inline void od_stat_free(od_stat_t *stat)
{
	for (size_t i = 0; i < QUANTILES_WINDOW; ++i) {
		td_free(stat->transaction_hgram[i]);
		td_free(stat->query_hgram[i]);
	}
}

static inline void od_stat_query_start(od_stat_state_t *state)
{
	if (!state->query_time_start) {
		state->query_time_start = machine_time_us();
	}

	if (!state->tx_time_start) {
		state->tx_time_start = machine_time_us();
	}
}

static inline void od_stat_wait_time(od_stat_t *stat, uint64_t wait_time_us)
{
	atomic_fetch_add_explicit(&stat->wait_time_us, wait_time_us,
				  memory_order_relaxed);
	atomic_fetch_add_explicit(&stat->count_wait, 1, memory_order_relaxed);
}

static inline void od_stat_parse(od_stat_t *stat)
{
	atomic_fetch_add_explicit(&stat->count_parse, 1, memory_order_relaxed);
}

static inline void od_stat_parse_reuse(od_stat_t *stat)
{
	atomic_fetch_add_explicit(&stat->count_parse_reuse, 1,
				  memory_order_relaxed);
}

static inline void od_stat_cancel(od_stat_t *stat)
{
	atomic_fetch_add_explicit(&stat->count_cancel, 1, memory_order_relaxed);
}

static inline void od_stat_query_end(od_stat_t *stat, od_stat_state_t *state,
				     int in_transaction, int64_t *query_time)
{
	int64_t diff;
	if (state->query_time_start) {
		diff = machine_time_us() - state->query_time_start;
		if (diff > 0) {
			*query_time = diff;
			atomic_fetch_add_explicit(&stat->query_time, diff,
						  memory_order_relaxed);
			atomic_fetch_add_explicit(&stat->count_query, 1,
						  memory_order_relaxed);
			if (stat->enable_quantiles) {
				td_add(stat->query_hgram[stat->current_tdigest],
				       diff, 1);
			}
		}
		state->query_time_start = 0;
	}

	if (in_transaction) {
		return;
	}

	if (state->tx_time_start) {
		diff = machine_time_us() - state->tx_time_start;
		if (diff > 0) {
			atomic_fetch_add_explicit(&stat->tx_time, diff,
						  memory_order_relaxed);
			atomic_fetch_add_explicit(&stat->count_tx, 1,
						  memory_order_relaxed);
			if (stat->enable_quantiles) {
				td_add(stat->transaction_hgram
					       [stat->current_tdigest],
				       diff, 1);
			}
		}
		state->tx_time_start = 0;
	}
}

static inline void od_stat_recv_server(od_stat_t *stat, uint64_t bytes)
{
	atomic_fetch_add_explicit(&stat->recv_server, bytes,
				  memory_order_relaxed);
}

static inline void od_stat_recv_client(od_stat_t *stat, uint64_t bytes)
{
	atomic_fetch_add_explicit(&stat->recv_client, bytes,
				  memory_order_relaxed);
}

static inline void od_stat_copy(od_stat_t *dst, od_stat_t *src)
{
	dst->count_query =
		atomic_load_explicit(&src->count_query, memory_order_relaxed);
	dst->count_tx =
		atomic_load_explicit(&src->count_tx, memory_order_relaxed);
	dst->query_time =
		atomic_load_explicit(&src->query_time, memory_order_relaxed);
	dst->tx_time =
		atomic_load_explicit(&src->tx_time, memory_order_relaxed);
	dst->count_wait =
		atomic_load_explicit(&src->count_wait, memory_order_relaxed);
	dst->wait_time_us =
		atomic_load_explicit(&src->wait_time_us, memory_order_relaxed);
	dst->recv_client =
		atomic_load_explicit(&src->recv_client, memory_order_relaxed);
	dst->recv_server =
		atomic_load_explicit(&src->recv_server, memory_order_relaxed);
	dst->count_parse =
		atomic_load_explicit(&src->count_parse, memory_order_relaxed);
	dst->count_parse_reuse = atomic_load_explicit(&src->count_parse_reuse,
						      memory_order_relaxed);
	dst->count_cancel =
		atomic_load_explicit(&src->count_cancel, memory_order_relaxed);
}

static inline void od_stat_sum(od_stat_t *sum, od_stat_t *stat)
{
	sum->count_query +=
		atomic_load_explicit(&stat->count_query, memory_order_relaxed);
	sum->count_tx +=
		atomic_load_explicit(&stat->count_tx, memory_order_relaxed);
	sum->query_time +=
		atomic_load_explicit(&stat->query_time, memory_order_relaxed);
	sum->tx_time +=
		atomic_load_explicit(&stat->tx_time, memory_order_relaxed);
	sum->count_wait +=
		atomic_load_explicit(&stat->count_wait, memory_order_relaxed);
	sum->wait_time_us +=
		atomic_load_explicit(&stat->wait_time_us, memory_order_relaxed);
	sum->recv_client +=
		atomic_load_explicit(&stat->recv_client, memory_order_relaxed);
	sum->recv_server +=
		atomic_load_explicit(&stat->recv_server, memory_order_relaxed);
	sum->count_parse +=
		atomic_load_explicit(&stat->count_parse, memory_order_relaxed);
	sum->count_parse_reuse += atomic_load_explicit(&stat->count_parse_reuse,
						       memory_order_relaxed);
	sum->count_cancel +=
		atomic_load_explicit(&stat->count_cancel, memory_order_relaxed);
}

static inline void od_stat_update_of(atomic_uint_fast64_t *prev,
				     atomic_uint_fast64_t *current)
{
	atomic_store_explicit(
		prev, atomic_load_explicit(current, memory_order_relaxed),
		memory_order_relaxed);
}

static inline void od_stat_update(od_stat_t *dst, od_stat_t *stat)
{
	od_stat_update_of(&dst->count_query, &stat->count_query);
	od_stat_update_of(&dst->count_tx, &stat->count_tx);
	od_stat_update_of(&dst->query_time, &stat->query_time);
	od_stat_update_of(&dst->tx_time, &stat->tx_time);
	od_stat_update_of(&dst->count_wait, &stat->count_wait);
	od_stat_update_of(&dst->wait_time_us, &stat->wait_time_us);
	od_stat_update_of(&dst->recv_client, &stat->recv_client);
	od_stat_update_of(&dst->recv_server, &stat->recv_server);
	od_stat_update_of(&dst->count_parse, &stat->count_parse);
	od_stat_update_of(&dst->count_parse_reuse, &stat->count_parse_reuse);
	od_stat_update_of(&dst->count_cancel, &stat->count_cancel);
}

static inline void od_stat_average(od_stat_t *avg, od_stat_t *current,
				   od_stat_t *prev, uint64_t prev_time_us)
{
	const uint64_t interval_usec = 1000000;
	uint64_t interval_us;
	interval_us = machine_time_us() - prev_time_us;
	if (interval_us <= 0) {
		return;
	}

	uint64_t count_query;
	count_query =
		atomic_load_explicit(&current->count_query,
				     memory_order_relaxed) -
		atomic_load_explicit(&prev->count_query, memory_order_relaxed);

	uint64_t count_tx;
	count_tx =
		atomic_load_explicit(&current->count_tx, memory_order_relaxed) -
		atomic_load_explicit(&prev->count_tx, memory_order_relaxed);

	uint64_t count_wait;
	count_wait =
		atomic_load_explicit(&current->count_wait,
				     memory_order_relaxed) -
		atomic_load_explicit(&prev->count_wait, memory_order_relaxed);

	uint64_t count_parse;
	count_parse =
		atomic_load_explicit(&current->count_parse,
				     memory_order_relaxed) -
		atomic_load_explicit(&prev->count_parse, memory_order_relaxed);

	uint64_t count_parse_reuse;
	count_parse_reuse = atomic_load_explicit(&current->count_parse_reuse,
						 memory_order_relaxed) -
			    atomic_load_explicit(&prev->count_parse_reuse,
						 memory_order_relaxed);

	avg->count_query = (count_query * interval_usec) / interval_us;
	avg->count_tx = (count_tx * interval_usec) / interval_us;
	avg->count_parse = (count_parse * interval_usec) / interval_us;
	avg->count_parse_reuse =
		(count_parse_reuse * interval_usec) / interval_us;

	if (count_query > 0) {
		avg->query_time = (atomic_load_explicit(&current->query_time,
							memory_order_relaxed) -
				   atomic_load_explicit(&prev->query_time,
							memory_order_relaxed)) /
				  count_query;
	}

	if (count_tx > 0) {
		avg->tx_time = (atomic_load_explicit(&current->tx_time,
						     memory_order_relaxed) -
				atomic_load_explicit(&prev->tx_time,
						     memory_order_relaxed)) /
			       count_tx;
	}

	if (count_wait > 0) {
		avg->wait_time_us =
			(atomic_load_explicit(&current->wait_time_us,
					      memory_order_relaxed) -
			 atomic_load_explicit(&prev->wait_time_us,
					      memory_order_relaxed)) /
			count_wait;
	}

	avg->recv_client = ((atomic_load_explicit(&current->recv_client,
						  memory_order_relaxed) -
			     atomic_load_explicit(&prev->recv_client,
						  memory_order_relaxed)) *
			    interval_usec) /
			   interval_us;

	avg->recv_server = ((atomic_load_explicit(&current->recv_server,
						  memory_order_relaxed) -
			     atomic_load_explicit(&prev->recv_server,
						  memory_order_relaxed)) *
			    interval_usec) /
			   interval_us;
}

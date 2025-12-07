#pragma once

#include <pthread.h>

#include <machinarium/machinarium.h>

typedef struct {
	uint64_t total;
	uint64_t idle;

	uint64_t total_diff;
	uint64_t idle_diff;
} od_cpu_stat_t;

typedef struct {
	uint64_t total;
	uint64_t available;
} od_mem_stat_t;

typedef struct {
	long hz;

	pthread_spinlock_t lock;
	od_cpu_stat_t cpu_stat;
	od_mem_stat_t mem_stat;

	machine_wait_flag_t *stop_flag;
	int64_t worker_id;
} od_host_watcher_t;

/* starts separated thread, because it uses file i/o */
int od_host_watcher_init(od_host_watcher_t *hw);
void od_host_watcher_destroy(od_host_watcher_t *hw);
void od_host_watcher_read(od_host_watcher_t *hw, float *cpu, float *mem);

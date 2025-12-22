#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <pthread.h>
#include <machinarium/machinarium.h>

#define OD_TLS_WATCHER_MAX_WATCHES 32

typedef struct {
	char *path;
	int wd; /* inotify watch descriptor */
} od_tls_watch_t;

typedef struct {
	int inotify_fd;
	int watch_count;
	od_tls_watch_t watches[OD_TLS_WATCHER_MAX_WATCHES];
	
	machine_wait_flag_t *stop_flag;
	int64_t worker_id;
	
	uint64_t last_reload_time_us; /* Timestamp of last reload for debouncing */
	
	void *system; /* od_system_t pointer to trigger reload */
} od_tls_watcher_t;

/* Initialize and start TLS watcher thread */
int od_tls_watcher_init(od_tls_watcher_t *watcher, void *system);

/* Add a file path to watch */
int od_tls_watcher_add(od_tls_watcher_t *watcher, const char *path);

/* Start the watcher thread */
int od_tls_watcher_start(od_tls_watcher_t *watcher);

/* Stop and cleanup TLS watcher */
void od_tls_watcher_destroy(od_tls_watcher_t *watcher);

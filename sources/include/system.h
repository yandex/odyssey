#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>
#include <machinarium/eventfd.h>
#include <machinarium/dns.h>
#include <machinarium/ds/vector.h>

#include <types.h>
#include <id.h>
#include <config.h>
#include <thread_pool.h>

struct od_system_server {
	mm_io_t *io;
	mm_eventfd_t shutdown_efd;
	machine_tls_t *tls;
	/* tls handles replaced by a reload, still referenced by live clients */
	mm_vector_t tls_retired;
	od_config_listen_t *config;
	mm_addrinfo_t *addr;
	od_global_t *global;
	od_list_t link;
	od_id_t sid;

	atomic_bool closed;

	int64_t coro_id;

	atomic_uint_fast64_t clients;
};

void od_system_server_free(od_system_server_t *server);
od_system_server_t *od_system_server_init(void);

void od_system_server_shutdown(od_system_server_t *server);

struct od_system {
	int64_t machine;
	int64_t sighandler_machine;
	machine_channel_t *system_channel;
	od_global_t *global;
	od_thread_pool_t reload_pool;
	/* Serialize request submission with closing the system channel. */
	pthread_mutex_t request_lock;
	bool requests_closed;
	size_t pending_signal_reloads;
};

typedef struct od_system_request od_system_request_t;

/* Caller and system each own a reference after successful submission. */
od_system_request_t *od_system_request_create(bool notify_ready);
void od_system_request_unref(od_system_request_t *request);
int od_system_request_submit(od_system_t *system, int type,
			     od_system_request_t *request);
int od_system_request_wait(od_system_request_t *request);
void od_system_requests_close(od_system_t *system);

od_system_t *od_system_create(void);
void od_system_init(od_system_t *);
int od_system_start(od_system_t *, od_global_t *);
void od_system_free(od_system_t *system);

int od_system_send_msg(od_system_t *system, int msg_type, void *arg);

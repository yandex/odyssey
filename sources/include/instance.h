#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <pthread.h>
#include <stdatomic.h>

#include <machinarium/ds/hm.h>

#include <kiwi/key.h>

#include <types.h>
#include <config.h>
#include <logger.h>
#include <pid.h>
#include <pstmt.h>

typedef struct timeval od_timeval_t;

struct od_instance {
	od_pid_t pid;
	od_logger_t logger;
	char *config_file;
	char *exec_path;
	od_config_t config;
	char *orig_argv_ptr;
	int orig_argv_ptr_len;
	atomic_int_fast64_t shutdown_worker_id;
	/* last config load failed, so a restart would not come up */
	atomic_int config_load_failed;
	struct {
		int argc;
		char **argv;
		char **envp;
	} cmdline;

	od_global_pstmt_map_t *pstmts;
	mm_hashmap_t *clients;
};

od_instance_t *od_instance_create(void);
void od_instance_free(od_instance_t *);
int od_instance_main(od_instance_t *instance, int argc, char **argv,
		     char **envp);

char *od_instance_getenv(od_instance_t *instance, const char *name);

void od_instance_set_shutdown_worker_id(od_instance_t *instance, int64_t id);
int64_t od_instance_get_shutdown_worker_id(od_instance_t *instance);

od_global_pstmt_map_t *od_instance_get_pstmts_map(od_instance_t *instance);

mm_hash_t od_instance_clients_hm_hash(const void *a);
int od_instance_clients_hm_cmp(const void *a, const void *b);
int od_instance_clients_lock(od_instance_t *instance, const kiwi_key_t *key,
			     mm_hashmap_keylock_t *klock);
void od_instance_clients_unlock(od_instance_t *instance,
				mm_hashmap_keylock_t *klock);
int od_instance_clients_add(od_instance_t *instance, od_client_t *client);
int od_instance_clients_remove(od_instance_t *instance, od_client_t *client);
int od_instance_clients_find(od_instance_t *instance, const kiwi_key_t *key,
			     mm_hashmap_keylock_t *klock, od_client_t **result);

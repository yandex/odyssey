#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <pthread.h>
#include <stdatomic.h>

#include <types.h>
#include <config.h>
#include <logger.h>
#include <pid.h>
#include <pstmt.h>
#include <clients_map.h>

typedef struct timeval od_timeval_t;

struct od_instance {
	od_pid_t pid;
	od_logger_t logger;
	char *config_file;
	char *exec_path;
	od_config_t config;
	char *orig_argv_ptr;
	int orig_argv_ptr_len;
	/* last config load failed, so a restart would not come up */
	atomic_int config_load_failed;
	struct {
		int argc;
		char **argv;
		char **envp;
	} cmdline;

	od_global_pstmt_map_t *pstmts;
	od_global_clients_map_t *clients_by_key;
};

od_instance_t *od_instance_create(void);
void od_instance_free(od_instance_t *);
int od_instance_main(od_instance_t *instance, int argc, char **argv,
		     char **envp);

od_global_pstmt_map_t *od_instance_get_pstmts_map(od_instance_t *instance);

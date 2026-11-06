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

typedef enum {
	OD_RESET_REASON_OOM = 0,
	OD_RESET_REASON_CACHED_PLAN_BROKEN,
	OD_RESET_REASON_COPY_MODE,
	OD_RESET_REASON_MSG_BROKEN,
	OD_RESET_REASON_XPROTO_MODE,
	OD_RESET_REASON_IN_TRANSACTION,
	OD_RESET_REASON_LEFT_IN_QUERY,
	OD_RESET_REASON_RFQ_WAIT_FAILED,
	OD_RESET_REASON_SYNC_FAILED,
	OD_RESET_REASON_COUNT
} od_reset_reason_t;

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
	atomic_uint_fast64_t reset_errors;
	atomic_uint_fast64_t reset_stats[OD_RESET_REASON_COUNT];
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

char *od_instance_getenv(od_instance_t *instance, const char *name);

void od_instance_reset_reason_inc(od_instance_t *instance,
				  od_reset_reason_t reason);
uint64_t od_instance_reset_reason_get(od_instance_t *instance,
				      od_reset_reason_t reason);
const char *od_reset_reason_to_str(od_reset_reason_t reason);

od_global_pstmt_map_t *od_instance_get_pstmts_map(od_instance_t *instance);

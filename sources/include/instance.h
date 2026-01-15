#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdatomic.h>

#include <types.h>
#include <config.h>
#include <logger.h>
#include <pid.h>

typedef struct timeval od_timeval_t;

struct od_instance {
	od_pid_t pid;
	od_logger_t logger;
	char *config_file;
	char *exec_path;
	od_config_t config;
	char *orig_argv_ptr;
	atomic_int_fast64_t shutdown_worker_id;
	struct {
		int argc;
		char **argv;
		char **envp;
	} cmdline;
};

od_instance_t *od_instance_create(void);
void od_instance_free(od_instance_t *);
int od_instance_main(od_instance_t *instance, int argc, char **argv,
		     char **envp);

char *od_instance_getenv(od_instance_t *instance, const char *name);

void od_instance_set_shutdown_worker_id(od_instance_t *instance, int64_t id);
int64_t od_instance_get_shutdown_worker_id(od_instance_t *instance);

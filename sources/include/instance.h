#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

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
	int64_t shutdown_worker_id;
};

od_instance_t *od_instance_create();
void od_instance_free(od_instance_t *);
int od_instance_main(od_instance_t *, int, char **);

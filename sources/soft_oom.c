/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium.h>
#include <odyssey.h>

#include <sys/types.h>
#include <dirent.h>

#define SOFT_OOM_LOG_CONTEXT "soft-oom"
#define PROC_DIR_PATH "/proc"
#define PROC_COMM_FMT "/proc/%d/comm"
#define PROC_SMAPS_ROLLUP "/proc/%d/smaps_rollup"
#define PROC_MEMINFO "/proc/meminfo"

static inline int od_soft_oom_get_system_memory_consumption(uint64_t *result)
{
	FILE *fp = fopen(PROC_MEMINFO, "r");
	if (!fp) {
		od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			  "can't open '%s': %s", PROC_MEMINFO, strerror(errno));
		return NOT_OK_RESPONSE;
	}

	char line[256];
	int64_t mem_available = 0, mem_total = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "MemAvailable: %ld kB", &mem_available) == 1) {
			continue;
		}

		if (sscanf(line, "MemTotal: %ld kB", &mem_total) == 1) {
			continue;
		}
	}

	fclose(fp);

	int64_t mem_used = mem_total - mem_available;

	if (mem_used >= 0) {
		*result = ((uint64_t)mem_used) * 1024 /* value was in kb */;

		od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			"updated memory consumption for system: %" PRIu64
			" bytes",
			*result);
	} else {
		od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			"got negative mem used: total = %ld, available = %ld",
			mem_total, mem_total);
		*result = 0;
	}

	return OK_RESPONSE;
}

static inline int od_soft_oom_is_target_process(od_config_soft_oom_t *config,
						pid_t pid, int *result)
{
	char comm_path[256];
	char comm[32];

	snprintf(comm_path, sizeof(comm_path), PROC_COMM_FMT, pid);

	FILE *fp = fopen(comm_path, "r");
	if (fp == NULL) {
		return NOT_OK_RESPONSE;
	}

	if (fgets(comm, sizeof(comm), fp) == NULL) {
		fclose(fp);
		return NOT_OK_RESPONSE;
	}

	*result = 0;

	if (strstr(comm, config->process) != NULL) {
		*result = 1;
	}

	fclose(fp);

	return OK_RESPONSE;
}

static inline int od_soft_oom_accumulate_process_pss(pid_t pid,
						     uint64_t *result)
{
	char smaps_path[256];
	snprintf(smaps_path, sizeof(smaps_path), PROC_SMAPS_ROLLUP, pid);

	FILE *smaps = fopen(smaps_path, "r");
	if (smaps == NULL) {
		return NOT_OK_RESPONSE;
	}

	char line[256];
	size_t pss_kb = 0;
	while (fgets(line, sizeof(line), smaps)) {
		if (sscanf(line, "Pss:%zu", &pss_kb) == 1) {
			break;
		}
	}
	fclose(smaps);

	*result += (pss_kb * 1024);

	return OK_RESPONSE;
}

static inline int
od_soft_oom_get_mem_consumption_from_processes(od_config_soft_oom_t *config,
					       uint64_t *result)
{
	DIR *proc_dir = opendir(PROC_DIR_PATH);
	if (proc_dir == NULL) {
		od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			  "can't open " PROC_DIR_PATH ": %s", strerror(errno));
		return NOT_OK_RESPONSE;
	}

	*result = 0;

	int npids = 0;

	while (1) {
		struct dirent *entry = readdir(proc_dir);
		if (entry == NULL) {
			break;
		}

		if (entry->d_type != DT_DIR) {
			continue;
		}

		pid_t pid = atoi(entry->d_name);
		if (pid <= 0) {
			continue;
		}

		int is_target = 0;
		if (od_soft_oom_is_target_process(config, pid, &is_target) !=
		    OK_RESPONSE) {
			od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
				  "can't check pid %d: %s", pid,
				  strerror(errno));
			continue;
		}

		if (!is_target) {
			continue;
		}

		if (od_soft_oom_accumulate_process_pss(pid, result) !=
		    OK_RESPONSE) {
			od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
				  "can't accumulate pss from pid %d: %s", pid,
				  strerror(errno));
			continue;
		}

		++npids;
	}

	od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
		"updated memory consumption for '%s' (%d pids): %" PRIu64
		" bytes",
		config->process, npids, *result);

	closedir(proc_dir);

	return OK_RESPONSE;
}

static inline int od_soft_oom_get_mem_consumption(od_config_soft_oom_t *config,
						  uint64_t *result)
{
	if (config->process[0]) {
		return od_soft_oom_get_mem_consumption_from_processes(config,
								      result);
	}

	return od_soft_oom_get_system_memory_consumption(result);
}

static inline void od_soft_oom_checker(void *arg)
{
	od_soft_oom_checker_t *checker = arg;

	while (1) {
		int rc = machine_wait_flag_wait(
			checker->stop_flag, checker->config->check_interval_ms);
		if (rc != -1 && machine_errno() != ETIMEDOUT) {
			od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
				"stop flag is set, exiting soft oom checker");
			break;
		}

		uint64_t used_mem;
		if (od_soft_oom_get_mem_consumption(checker->config,
						    &used_mem) != OK_RESPONSE) {
			od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
				  "memory state update failed");
			continue;
		}

		atomic_store(&checker->current_memory_usage, used_mem);
	}
}

int od_soft_oom_start_checker(od_config_soft_oom_t *config,
			      od_soft_oom_checker_t *checker)
{
	machine_wait_flag_t *stop_flag = machine_wait_flag_create();
	if (stop_flag == NULL) {
		return NOT_OK_RESPONSE;
	}

	atomic_store(&checker->current_memory_usage, 0);
	checker->stop_flag = stop_flag;
	checker->config = config;

	/*
	 * yes, we pass not fully-initialized checker into thread
	 * but that is ok because we dont use machine_id inside new thread
	 */
	checker->machine_id =
		machine_create("soft-oom-worker", od_soft_oom_checker, checker);
	if (checker->machine_id == -1) {
		od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			  "can't create machine for soft oom checks");
		machine_wait_flag_destroy(checker->stop_flag);
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

void od_soft_oom_stop_checker(od_soft_oom_checker_t *checker)
{
	if (checker->machine_id == 0) {
		return;
	}

	machine_wait_flag_set(checker->stop_flag);

	machine_wait(checker->machine_id);

	machine_wait_flag_destroy(checker->stop_flag);

	checker->current_memory_usage = 0;
	checker->machine_id = 0;
	checker->stop_flag = NULL;
}
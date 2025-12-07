/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <sys/types.h>
#include <dirent.h>
#include <signal.h>

#include <machinarium/machinarium.h>

#include <soft_oom.h>
#include <config.h>
#include <od_memory.h>

#define SOFT_OOM_LOG_CONTEXT "soft-oom"
#define PROC_DIR_PATH "/proc"
#define PROC_COMM_FMT "/proc/%d/comm"
#define PROC_SMAPS_ROLLUP "/proc/%d/smaps_rollup"
#define PROC_MEMINFO "/proc/meminfo"
#define POSTGRES_COMM "postgres"

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
		*result = ((uint64_t)mem_used);

		od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			"updated memory consumption for system: %" PRIu64 " KB",
			*result);
	} else {
		od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			"got negative mem used: total = %ld, available = %ld",
			mem_total, mem_available);
		*result = 0;
	}

	return OK_RESPONSE;
}

static inline void strstrip_right(char *s)
{
	size_t size;
	char *end;
	size = strlen(s);
	if (!size) {
		return;
	}
	end = s + size - 1;
	while (end >= s && isspace(*end)) {
		end--;
	}
	*(end + 1) = '\0';
}

static inline int od_soft_oom_is_target_process(const char *target_comm,
						pid_t pid, char *pid_comm,
						int *result)
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

	if (strstr(comm, target_comm) != NULL) {
		*result = 1;
		strstrip_right(comm);
		strcpy(pid_comm, comm);
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

typedef int (*od_soft_oom_proc_cb)(pid_t pid, const char *comm, void *arg);

static inline int od_soft_oom_iterate_procs_by_comm(const char *comm,
						    od_soft_oom_proc_cb cb,
						    void *arg)
{
	DIR *proc_dir = opendir(PROC_DIR_PATH);
	if (proc_dir == NULL) {
		od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			  "can't open " PROC_DIR_PATH ": %s", strerror(errno));
		return NOT_OK_RESPONSE;
	}

	int rc = OK_RESPONSE;

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
		char pid_comm[32];
		if (od_soft_oom_is_target_process(comm, pid, pid_comm,
						  &is_target) != OK_RESPONSE) {
			od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
				  "can't check pid %d: %s", pid,
				  strerror(errno));
			continue;
		}

		if (!is_target) {
			continue;
		}

		rc = cb(pid, pid_comm, arg);

		if (rc != OK_RESPONSE) {
			break;
		}
	}

	closedir(proc_dir);

	return rc;
}

typedef struct {
	uint64_t *result;
	int npids;
} aggregate_pss_cb_arg_t;

static inline int
od_soft_oom_aggregate_consumption_cb(pid_t pid, const char *comm, void *arg)
{
	aggregate_pss_cb_arg_t *carg = arg;

	if (od_soft_oom_accumulate_process_pss(pid, carg->result) !=
	    OK_RESPONSE) {
		od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			  "can't accumulate pss from pid %d (%s): %s", pid,
			  comm, strerror(errno));
		/* return ok in order to not stop on errors */
		return OK_RESPONSE;
	}

	++carg->npids;

	return OK_RESPONSE;
}

typedef struct {
	uint64_t pss;
	char comm[32];
	pid_t pid;
} proc_pss_info_t;

int proc_pss_info_desc_cmp(const void *a, const void *b)
{
	const proc_pss_info_t *aa = a;
	const proc_pss_info_t *bb = b;

	/* can't do substraction here because numbers are unsigned? */

	if (aa->pss > bb->pss) {
		return -1;
	}

	if (aa->pss < bb->pss) {
		return 1;
	}

	return 0;
}

typedef struct {
	proc_pss_info_t *infos;
	size_t count;
	size_t capacity;
} list_procs_arg_t;

static inline void list_procs_arg_init(list_procs_arg_t *arg)
{
	arg->capacity = 0;
	arg->count = 0;
	arg->infos = NULL;
}

static inline void list_procs_arg_destroy(list_procs_arg_t *arg)
{
	od_free(arg->infos);
}

static inline int list_procs_add(list_procs_arg_t *arg, pid_t pid,
				 const char *comm, uint64_t pss)
{
	if (arg->count >= arg->capacity) {
		arg->capacity = arg->capacity ? (arg->capacity * 2) : 32;
		arg->infos = od_realloc(
			arg->infos, arg->capacity * sizeof(proc_pss_info_t));
		if (arg->infos == NULL) {
			return NOT_OK_RESPONSE;
		}
	}

	proc_pss_info_t *info = &arg->infos[arg->count++];

	info->pid = pid;
	info->pss = pss;
	strcpy(info->comm, comm);

	return OK_RESPONSE;
}

static inline int od_soft_oom_list_procs_cb(pid_t pid, const char *comm,
					    void *arg)
{
	list_procs_arg_t *larg = arg;

	uint64_t pss = 0;

	if (od_soft_oom_accumulate_process_pss(pid, &pss) != OK_RESPONSE) {
		od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			  "can't accumulate pss from pid %d: %s", pid,
			  strerror(errno));
		/* return ok in order to not stop on errors of pss accumulate */
		return OK_RESPONSE;
	}

	/* can return not ok - any next add will fail if so */
	return list_procs_add(larg, pid, comm, pss);
}

static inline int
od_soft_oom_get_mem_consumption_from_processes(od_config_soft_oom_t *config,
					       uint64_t *result)
{
	aggregate_pss_cb_arg_t arg = { .npids = 0, .result = result };

	int rc = od_soft_oom_iterate_procs_by_comm(
		config->process, od_soft_oom_aggregate_consumption_cb, &arg);

	od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
		"updated memory consumption for '%s' (%d pids): %lu bytes",
		config->process, arg.npids, *result);

	return rc;
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

static inline void od_soft_oom_signal_postgres(od_soft_oom_checker_t *checker)
{
	od_config_soft_oom_t *config = checker->config;
	od_config_soft_oom_drop_t *drop = &checker->config->drop;

	if (!drop->enabled) {
		return;
	}

	uint64_t used_bytes = 0;
	if (!od_soft_oom_is_in_soft_oom(checker, &used_bytes)) {
		return;
	}

	od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
		"used memory (%lu) >= limit (%lu), need to signal %d to top %d pss consumers",
		used_bytes, config->limit_bytes, drop->signal, drop->max_rate);

	list_procs_arg_t arg;
	list_procs_arg_init(&arg);

	int rc = od_soft_oom_iterate_procs_by_comm(
		POSTGRES_COMM, od_soft_oom_list_procs_cb, &arg);
	if (rc != OK_RESPONSE) {
		od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			  "fail to build pss list");
		list_procs_arg_destroy(&arg);
		return;
	}

	qsort(arg.infos, arg.count, sizeof(proc_pss_info_t),
	      proc_pss_info_desc_cmp);

	for (size_t i = 0; i < arg.count && i < (size_t)drop->max_rate; ++i) {
		proc_pss_info_t *info = &arg.infos[i];

		od_glog(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
			"sending %d signal to %d (%s)...", drop->signal,
			info->pid, info->comm);
		if (kill(info->pid, drop->signal) == -1) {
			od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
				  "can't send signal to %d: %s", info->pid,
				  strerror(errno));
		}
	}

	list_procs_arg_destroy(&arg);
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

		uint64_t used_mem = 0;
		if (od_soft_oom_get_mem_consumption(checker->config,
						    &used_mem) != OK_RESPONSE) {
			od_gerror(SOFT_OOM_LOG_CONTEXT, NULL, NULL,
				  "memory state update failed");
			continue;
		}

		atomic_store(&checker->current_memory_usage, used_mem);

		od_soft_oom_signal_postgres(checker);
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

int od_soft_oom_is_in_soft_oom(od_soft_oom_checker_t *checker,
			       uint64_t *used_memory)
{
	*used_memory = atomic_load(&checker->current_memory_usage);

	return *used_memory >= checker->config->limit_bytes;
}

#include <unistd.h>
#include <string.h>
#include <host_watcher.h>
#include <odyssey.h>

#define PROC_MEMINFO_PATH "/proc/meminfo"
#define PROC_STAT_PATH "/proc/stat"

DEFINE_SIMPLE_MODULE_LOGGER(hw, "host-watcher")

static inline uint64_t sub(uint64_t a, uint64_t b)
{
	return a > b ? a - b : 0;
}

static inline void hw_update_cpu_stat(od_host_watcher_t *hw,
				      const od_cpu_stat_t *stat)
{
	hw->cpu_stat.idle_diff = sub(stat->idle, hw->cpu_stat.idle);
	hw->cpu_stat.total_diff = sub(stat->total, hw->cpu_stat.total);
	hw->cpu_stat.idle = stat->idle;
	hw->cpu_stat.total = stat->total;
}

static inline void hw_update_mem_stat(od_host_watcher_t *hw,
				      const od_mem_stat_t *stat)
{
	memcpy(&hw->mem_stat, stat, sizeof(od_mem_stat_t));
}

static inline void hw_update(od_host_watcher_t *hw, const od_cpu_stat_t *cpu,
			     const od_mem_stat_t *mem)
{
	pthread_spin_lock(&hw->lock);

	hw_update_cpu_stat(hw, cpu);
	hw_update_mem_stat(hw, mem);

	pthread_spin_unlock(&hw->lock);
}

static inline int get_cpu_stat(od_cpu_stat_t *out)
{
	/* https://www.kernel.org/doc/html/latest/filesystems/proc.html#miscellaneous-kernel-statistics-in-proc-stat */

	FILE *fp = fopen(PROC_STAT_PATH, "r");
	if (!fp) {
		hw_error("can't open '%s': %s", PROC_STAT_PATH,
			 strerror(errno));
		return -1;
	}

	static OD_THREAD_LOCAL char cpu_stat_buffer[4096 + 1];

	if (fgets(cpu_stat_buffer, sizeof(cpu_stat_buffer), fp) == NULL) {
		hw_error("can't read '%s': %s", PROC_STAT_PATH,
			 strerror(errno));
		return -1;
	}

	fclose(fp);

	uint64_t usertime, nicetime, systemtime, idletime;
	uint64_t io_wait = 0, irq = 0, soft_irq = 0, steal = 0, guest = 0,
		 guestnice = 0;

	sscanf(cpu_stat_buffer, "cpu  %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
	       &usertime, &nicetime, &systemtime, &idletime, &io_wait, &irq,
	       &soft_irq, &steal, &guest, &guestnice);

	usertime -= guest;
	nicetime -= guestnice;

	uint64_t idle_time = idletime + io_wait;
	uint64_t system_time = systemtime + irq + soft_irq;
	uint64_t vm_time = guest + guestnice;
	uint64_t total_time =
		usertime + nicetime + system_time + idle_time + steal + vm_time;

	out->total = total_time;
	out->idle = idle_time;

	return 0;
}

static inline int get_memory_stat(od_mem_stat_t *out)
{
	/* https://www.kernel.org/doc/html/latest/filesystems/proc.html#meminfo */

	FILE *fp = fopen(PROC_MEMINFO_PATH, "r");
	if (!fp) {
		hw_error("can't open '%s': %s", PROC_MEMINFO_PATH,
			 strerror(errno));
		return -1;
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

	if (ferror(fp)) {
		hw_error("can't read '%s': %s", PROC_MEMINFO_PATH,
			 strerror(errno));
		fclose(fp);
		return -1;
	}

	fclose(fp);

	out->total = mem_total;
	out->available = mem_available;

	return 0;
}

static inline void hw_watcher(void *arg)
{
	od_host_watcher_t *hw = arg;

	hw_log("host watcher started");

	while (1) {
		int rc = machine_wait_flag_wait(hw->stop_flag, 1000);
		if (rc != -1 && machine_errno() != ETIMEDOUT) {
			hw_log("stop flag is set, exiting host watcher");
			break;
		}

		od_mem_stat_t mem;
		if (get_memory_stat(&mem)) {
			continue;
		}

		od_cpu_stat_t cpu;
		if (get_cpu_stat(&cpu)) {
			continue;
		}

		hw_update(hw, &cpu, &mem);

		float ucpu, umem;
		od_host_watcher_read(hw, &ucpu, &umem);

		hw_log("update values to: %.2f%% cpu and %.2f%% mem", ucpu,
		       umem);
	}

	hw_log("host watcher finished");
}

int od_host_watcher_init(od_host_watcher_t *hw)
{
	memset(&hw->cpu_stat, 0, sizeof(od_cpu_stat_t));
	memset(&hw->mem_stat, 0, sizeof(od_mem_stat_t));

	pthread_spin_init(&hw->lock, PTHREAD_PROCESS_PRIVATE);

	hw->hz = sysconf(_SC_CLK_TCK);
	if (hw->hz == -1) {
		return -1;
	}

	hw->stop_flag = machine_wait_flag_create();
	if (hw->stop_flag == NULL) {
		pthread_spin_destroy(&hw->lock);
		return -1;
	}

	hw->worker_id = machine_create("host-watcher", hw_watcher, hw);
	if (hw->worker_id == -1) {
		hw_error("can't start host watcher machine");
		machine_wait_flag_destroy(hw->stop_flag);
		pthread_spin_destroy(&hw->lock);
		return -1;
	}

	return 0;
}

void od_host_watcher_destroy(od_host_watcher_t *hw)
{
	machine_wait_flag_set(hw->stop_flag);
	machine_wait(hw->worker_id);
	machine_wait_flag_destroy(hw->stop_flag);

	pthread_spin_destroy(&hw->lock);
}

static inline float get_mem_util(const od_host_watcher_t *hw)
{
	return (float)(hw->mem_stat.total - hw->mem_stat.available) /
	       hw->mem_stat.total * 100.0;
}

static inline float get_cpu_util(const od_host_watcher_t *hw)
{
	return (float)(hw->cpu_stat.total_diff - hw->cpu_stat.idle_diff) /
	       hw->cpu_stat.total_diff * 100.0;
}

void od_host_watcher_read(od_host_watcher_t *hw, float *cpu, float *mem)
{
	pthread_spin_lock(&hw->lock);
	*cpu = get_cpu_util(hw);
	*mem = get_mem_util(hw);
	pthread_spin_unlock(&hw->lock);
}
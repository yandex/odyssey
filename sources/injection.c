/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Test injection points registry.
 */

#include <odyssey.h>

#include <pthread.h>
#include <stdatomic.h>

#include <machinarium/machinarium.h>

#include <injection.h>
#include <logger.h>
#include <util.h>

#define OD_INJECTION_POINTS_MAX 8

typedef struct {
	char name[64];
	atomic_bool loaded;
	atomic_uint waiters;
} od_injection_point_t;

static od_injection_point_t od_injection_points[OD_INJECTION_POINTS_MAX];
static pthread_mutex_t od_injection_lock = PTHREAD_MUTEX_INITIALIZER;

static od_injection_point_t *od_injection_find(const char *name)
{
	for (int i = 0; i < OD_INJECTION_POINTS_MAX; i++) {
		if (od_injection_points[i].name[0] != 0 &&
		    strcmp(od_injection_points[i].name, name) == 0) {
			return &od_injection_points[i];
		}
	}
	return NULL;
}

int od_injection_load(const char *name)
{
	pthread_mutex_lock(&od_injection_lock);
	od_injection_point_t *ip = od_injection_find(name);
	if (ip != NULL) {
		bool loaded = atomic_load(&ip->loaded);
		/* Do not re-arm until the previous waiters have left. */
		if (!loaded && atomic_load(&ip->waiters) != 0) {
			pthread_mutex_unlock(&od_injection_lock);
			return -1;
		}
		atomic_store(&ip->loaded, !loaded);
		pthread_mutex_unlock(&od_injection_lock);
		return 0;
	}

	if (strlen(name) >= sizeof(od_injection_points[0].name)) {
		pthread_mutex_unlock(&od_injection_lock);
		return -1;
	}
	for (int i = 0; i < OD_INJECTION_POINTS_MAX; i++) {
		ip = &od_injection_points[i];
		if (ip->name[0] != 0) {
			continue;
		}
		od_snprintf(ip->name, sizeof(ip->name), "%s", name);
		atomic_init(&ip->waiters, 0);
		atomic_init(&ip->loaded, true);
		pthread_mutex_unlock(&od_injection_lock);
		return 0;
	}
	pthread_mutex_unlock(&od_injection_lock);
	return -1;
}

int od_injection_await(const char *name, unsigned timeout_ms)
{
	pthread_mutex_lock(&od_injection_lock);
	od_injection_point_t *ip = od_injection_find(name);
	pthread_mutex_unlock(&od_injection_lock);
	if (ip == NULL) {
		return -1;
	}

	uint64_t deadline = machine_time_ms() + timeout_ms;
	while (atomic_load(&ip->waiters) == 0) {
		if (machine_time_ms() >= deadline) {
			return -1;
		}
		machine_sleep(1);
	}
	return 0;
}

void od_injection_wait(const char *name)
{
	pthread_mutex_lock(&od_injection_lock);
	od_injection_point_t *ip = od_injection_find(name);
	if (ip == NULL || !atomic_load(&ip->loaded)) {
		pthread_mutex_unlock(&od_injection_lock);
		return;
	}
	atomic_fetch_add(&ip->waiters, 1);
	pthread_mutex_unlock(&od_injection_lock);

	od_log(NULL, NULL, NULL, "injection %s: parking", name);

	while (atomic_load(&ip->loaded)) {
		machine_sleep(1);
	}
	atomic_fetch_sub(&ip->waiters, 1);
	od_log(NULL, NULL, NULL, "injection %s: released", name);
}

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Test injection points registry.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <injection.h>
#include <logger.h>
#include <util.h>

#define OD_INJECTION_POINTS_MAX 8
#define OD_INJECTION_RELEASE_TOKENS 1000

static od_injection_point_t od_injection_points[OD_INJECTION_POINTS_MAX];

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

void od_injection_load(const char *name)
{
	od_injection_point_t *ip = od_injection_find(name);
	if (ip != NULL) {
		/* already loaded - release parked waiters */
		od_log(NULL, "dbginject", NULL, NULL, "injection %s: release",
		       name);
		ip->loaded = false;
		atomic_store_explicit(&ip->tokens, OD_INJECTION_RELEASE_TOKENS,
				      memory_order_release);
		return;
	}

	for (int i = 0; i < OD_INJECTION_POINTS_MAX; i++) {
		ip = &od_injection_points[i];
		if (ip->name[0] != 0) {
			continue;
		}
		od_snprintf(ip->name, sizeof(ip->name), "%s", name);
		atomic_init(&ip->tokens, 0);
		ip->loaded = true;
		return;
	}
}

void od_injection_wait(const char *name)
{
	od_injection_point_t *ip = od_injection_find(name);
	if (ip == NULL || !ip->loaded) {
		return;
	}

	od_log(NULL, "dbginject", NULL, NULL, "injection %s: parking", name);

	for (;;) {
		uint64_t v =
			atomic_load_explicit(&ip->tokens, memory_order_acquire);
		if (v == 0) {
			machine_sleep(1);
			continue;
		}
		if (atomic_compare_exchange_weak_explicit(
			    &ip->tokens, &v, v - 1, memory_order_acq_rel,
			    memory_order_acquire)) {
			od_log(NULL, "dbginject", NULL, NULL,
			       "injection %s: released", name);
			return;
		}
	}
}

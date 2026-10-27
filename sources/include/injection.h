#pragma once

/*
 * Odyssey.
 *
 * Test injection points.
 */

#include <build.h>

#ifdef ENABLE_INJECTION_POINTS

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

typedef struct {
	char name[64];
	bool loaded;
	atomic_uint_fast64_t tokens;
} od_injection_point_t;

void od_injection_load(const char *name);
void od_injection_wait(const char *name);

#else

#define od_injection_wait(name) ((void)0)

#endif

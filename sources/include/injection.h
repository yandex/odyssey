#pragma once

/*
 * Odyssey.
 *
 * Test injection points.
 */

#include <build.h>

#ifdef ENABLE_INJECTION_POINTS

/* LOAD toggles a point; WAIT acknowledges a parked coroutine. */
int od_injection_load(const char *name);
int od_injection_await(const char *name, unsigned timeout_ms);
void od_injection_wait(const char *name);

#else

#define od_injection_wait(name) ((void)0)

#endif

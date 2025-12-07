#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

#define ODYSSEY_WATCHDOG_ITER_INTERVAL 500 /* ms */

void od_watchdog_worker(void *arg);

od_retcode_t od_watchdog_invoke(od_system_t *server);

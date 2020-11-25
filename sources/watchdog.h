#ifndef ODYSSEY_WATCHDOG_H
#define ODYSSEY_WATCHDOG_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/file.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <signal.h>

#include <machinarium.h>

#include "debugprintf.h"
#include "instance.h"
#include "kiwi.h"
#include "logger.h"
#include "macro.h"
#include "pid.h"
#include "restart_sync.h"
#include "setproctitle.h"
#include "system.h"

#define ODYSSEY_WATCHDOG_ITER_INTERVAL 500 // ms

void
od_watchdog_worker(void *arg);

od_retcode_t
od_watchdog_invoke(od_system_t *server);

#endif /* ODYSSEY_WATCHDOG_H */

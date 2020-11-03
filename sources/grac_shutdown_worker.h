#ifndef ODYSSEY_GRAC_KILLER_H
#define ODYSSEY_GRAC_KILLER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <string.h>
#include <signal.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

void
od_grac_shutdown_worker(void *arg);

#endif /* ODYSSEY_GRAC_KILLER_H */

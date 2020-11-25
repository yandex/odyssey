#ifndef ODYSSEY_GRAC_KILLER_H
#define ODYSSEY_GRAC_KILLER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <signal.h>
#include <string.h>

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

void
od_grac_shutdown_worker(void *arg);

#endif /* ODYSSEY_GRAC_KILLER_H */

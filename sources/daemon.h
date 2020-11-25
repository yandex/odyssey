#ifndef ODYSSEY_DAEMON_H
#define ODYSSEY_DAEMON_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <fcntl.h>
#include <odyssey.h>
#include <unistd.h>

int
od_daemonize(void);

#endif /* ODYSSEY_DAEMON_H */

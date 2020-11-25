#ifndef ODYSSEY_CONSOLE_H
#define ODYSSEY_CONSOLE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "c.h"
#include <kiwi.h>
#include <machinarium.h>
#include <math.h>
#include <odyssey.h>

int
od_console_query(od_client_t *, machine_msg_t *, char *, uint32_t);

#endif /* ODYSSEY_CONSOLE_H */

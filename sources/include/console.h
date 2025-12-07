#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <types.h>

int od_console_query(od_client_t *, machine_msg_t *, char *, uint32_t);

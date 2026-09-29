#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

#define ODYSSEY_AUTH_QUERY_MAX_PASSWORD_LEN 4096

int od_auth_query(od_client_t *, char *);
void od_auth_query_cache_free(od_route_t *);

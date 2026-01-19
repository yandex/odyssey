#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>
#include <hashmap.h>

#define ODYSSEY_AUTH_QUERY_MAX_PASSWORD_LEN 4096

od_hashmap_t *od_auth_query_create_cache(size_t sz);

int od_auth_query(od_client_t *, char *);

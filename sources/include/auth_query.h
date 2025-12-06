#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#define ODYSSEY_AUTH_QUERY_MAX_PASSWORD_LEN 4096

int od_auth_query(od_client_t *, char *);

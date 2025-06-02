#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_parse_endpoints(const char *host_str, od_storage_endpoint_t **out,
		       size_t *count);

#ifndef ODYSSEY_ENDPOINTS_READER_H
#define ODYSSEY_ENDPOINTS_READER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_parse_endpoints(const char *host_str, od_storage_endpoint_t **out,
		       size_t *count);

#endif /* ODYSSEY_ENDPOINTS_READER_H */
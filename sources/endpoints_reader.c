#ifndef ODYSSEY_ENDPOINTS_READER_H
#define ODYSSEY_ENDPOINTS_READER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

static inline int
od_config_reader_endpoint_host(char *host, od_storage_endpoint_t *endpoint)
{
	int host_len = strlen(host);
	if (host_len <= 0) {
		return NOT_OK_RESPONSE;
	}

	if (host[0] == '[') {
		if (host[host_len - 1] != ']') {
			return NOT_OK_RESPONSE;
		}

		++host;
		host_len -= 2;

		if (host_len == 0) {
			return NOT_OK_RESPONSE;
		}
	}

	endpoint->host = malloc(host_len + 1);
	if (endpoint->host == NULL) {
		return NOT_OK_RESPONSE;
	}
	memcpy(endpoint->host, host, host_len);
	endpoint->host[host_len] = 0;

	return OK_RESPONSE;
}

static inline int
od_config_reader_endpoint_port(const char *port,
			       od_storage_endpoint_t *endpoint)
{
	if (endpoint->port != 0) {
		/* can not set twice */
		return NOT_OK_RESPONSE;
	}

	errno = 0; /* to distinguish success/failure */
	int val = strtol(port, NULL, 10);
	if (errno == ERANGE || errno == EINVAL) {
		return NOT_OK_RESPONSE;
	}

	if (val <= 0 || val > (1 << 16)) {
		return NOT_OK_RESPONSE;
	}

	endpoint->port = val;

	return OK_RESPONSE;
}

static inline int
od_config_reader_endpoint_zone(const char *zone,
			       od_storage_endpoint_t *endpoint)
{
	if (strlen(endpoint->availability_zone) != 0) {
		/* can not set twice */
		return NOT_OK_RESPONSE;
	}

	int len = strlen(zone);
	if (len > MAX_ENDPOINT_AVAILABILITY_ZONE_LENGTH - 1 /* 0-byte */) {
		return NOT_OK_RESPONSE;
	}

	strcpy(endpoint->availability_zone, zone);

	return OK_RESPONSE;
}

static inline int
od_config_reader_parse_endpoint(char *buff, od_storage_endpoint_t *endpoint)
{
	char *strtok_preserve = NULL;

	endpoint->host = NULL;
	endpoint->port = 0;
	memset(endpoint->availability_zone, 0,
	       sizeof(endpoint->availability_zone));

	char *token = strtok_r(buff, ":", &strtok_preserve);
	if (token == NULL) {
		goto error;
	}

	if (od_config_reader_endpoint_host(token, endpoint) != OK_RESPONSE) {
		goto error;
	}

	token = strtok_r(NULL, ":", &strtok_preserve);
	while (token != NULL) {
		if (strlen(token) == 0) {
			goto error;
		}

		if (isdigit(token[0])) {
			if (od_config_reader_endpoint_port(token, endpoint) !=
			    OK_RESPONSE) {
				goto error;
			}
		} else if (od_config_reader_endpoint_zone(token, endpoint) !=
			   OK_RESPONSE) {
			goto error;
		}

		token = strtok_r(NULL, ":", &strtok_preserve);
	}

	return OK_RESPONSE;

error:
	free(endpoint->host);
	return NOT_OK_RESPONSE;
}

size_t od_config_reader_get_endpoints_count(const char *buff, int len)
{
	size_t count = 1;
	for (int i = 0; i < len; ++i) {
		if (buff[i] == ',') {
			++count;
		}
	}

	return count;
}

int od_parse_endpoints(const char *host_str, od_storage_endpoint_t **out,
		       size_t *count)
{
	/*
	 * parse strings like 'host(,host)*' where host is:
	 * [address](:port)?(:availability_zone)?
	 * examples:
	 * klg-hostname.com
	 * [klg-hostname.com]:1337
	 * [klg-hostname.com]:klg
	 * [klg-hostname.com]:1337:klg
	 * klg-hostname.com:1337:klg
	 * [klg-hostname.com]:klg:1337
	 * klg-hostname.com,vla-hostname.com
	 * klg-hostname.com,[vla-hostname.com]:31337
	 * [klg-hostname.com]:1337:klg,[vla-hostname.com]:31337:vla
	 */

	static __thread char buff[4096];
	char *strtok_preserve = NULL;

	int len = strlen(host_str);
	if (len > (int)sizeof(buff) - 1 /* 0-byte */) {
		return NOT_OK_RESPONSE;
	}
	strcpy(buff, host_str);

	size_t result_count = od_config_reader_get_endpoints_count(buff, len);
	od_storage_endpoint_t *result =
		malloc(result_count * sizeof(od_storage_endpoint_t));
	if (result == NULL) {
		return NOT_OK_RESPONSE;
	}
	od_storage_endpoint_t *endpoint = result;

	char *next_address = strtok_r(buff, ",", &strtok_preserve);
	while (next_address != NULL) {
		if (od_config_reader_parse_endpoint(next_address, endpoint) !=
		    OK_RESPONSE) {
			free(result);
			return NOT_OK_RESPONSE;
		}

		endpoint++;
		next_address = strtok_r(NULL, ",", &strtok_preserve);
	}

	*out = result;
	*count = result_count;

	return OK_RESPONSE;
}

#endif /* ODYSSEY_ENDPOINTS_READER_H */
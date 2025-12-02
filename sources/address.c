/*
* Odyssey.
*
* Scalable PostgreSQL connection pooler.
*/

#include <machinarium/machinarium.h>
#include <odyssey.h>

od_address_range_t od_address_range_create_default()
{
	od_address_range_t address_range = { .string_value = strdup("all"),
					     .string_value_len = strlen("all"),
					     .is_default = 1 };
	return address_range;
}

void od_address_range_destroy(od_address_range_t *range)
{
	od_free(range->string_value);
}

int od_address_range_copy(const od_address_range_t *src,
			  od_address_range_t *dst)
{
	dst->string_value = strndup(src->string_value, src->string_value_len);
	if (dst->string_value == NULL) {
		return 1;
	}
	dst->string_value_len = src->string_value_len;
	dst->addr = src->addr;
	dst->mask = src->mask;
	dst->is_default = src->is_default;
	dst->is_hostname = src->is_hostname;

	return 0;
}

int od_address_range_read_prefix(od_address_range_t *address_range,
				 char *prefix)
{
	char *end = NULL;
	long len = strtol(prefix, &end, 10);
	if (*prefix == '\0' || *end != '\0') {
		return -1;
	}
	if (address_range->addr.ss_family == AF_INET) {
		if (len > 32)
			return -1;
		struct sockaddr_in *addr =
			(struct sockaddr_in *)&address_range->mask;
		uint32_t mask;
		if (len > 0)
			mask = 0xffffffffUL << (32 - (int)len);
		else
			mask = 0;
		addr->sin_addr.s_addr = od_bswap32(mask);
		return 0;
	} else if (address_range->addr.ss_family == AF_INET6) {
		if (len > 128)
			return -1;
		struct sockaddr_in6 *addr =
			(struct sockaddr_in6 *)&address_range->mask;
		int i;
		for (i = 0; i < 16; i++) {
			if (len <= 0)
				addr->sin6_addr.s6_addr[i] = 0;
			else if (len >= 8)
				addr->sin6_addr.s6_addr[i] = 0xff;
			else {
				addr->sin6_addr.s6_addr[i] =
					(0xff << (8 - (int)len)) & 0xff;
			}
			len -= 8;
		}
		return 0;
	}

	return -1;
}

int od_address_read(struct sockaddr_storage *dest, const char *addr)
{
	int rc;
	rc = inet_pton(AF_INET, addr, &((struct sockaddr_in *)dest)->sin_addr);
	if (rc > 0) {
		dest->ss_family = AF_INET;
		return 0;
	}
	if (inet_pton(AF_INET6, addr,
		      &((struct sockaddr_in6 *)dest)->sin6_addr) > 0) {
		dest->ss_family = AF_INET6;
		return 0;
	}
	return -1;
}

static bool od_address_ipv4eq(struct sockaddr_in *a, struct sockaddr_in *b)
{
	return (a->sin_addr.s_addr == b->sin_addr.s_addr);
}

static bool od_address_ipv6eq(struct sockaddr_in6 *a, struct sockaddr_in6 *b)
{
	int i;
	for (i = 0; i < 16; i++)
		if (a->sin6_addr.s6_addr[i] != b->sin6_addr.s6_addr[i])
			return false;
	return true;
}

bool od_address_equals(struct sockaddr *firstAddress,
		       struct sockaddr *secondAddress)
{
	if (firstAddress->sa_family == secondAddress->sa_family) {
		if (firstAddress->sa_family == AF_INET) {
			if (od_address_ipv4eq(
				    (struct sockaddr_in *)firstAddress,
				    (struct sockaddr_in *)secondAddress))
				return true;
		} else if (firstAddress->sa_family == AF_INET6) {
			if (od_address_ipv6eq(
				    (struct sockaddr_in6 *)firstAddress,
				    (struct sockaddr_in6 *)secondAddress))
				return true;
		}
	}
	return false;
}

bool od_address_range_equals(const od_address_range_t *first,
			     const od_address_range_t *second)
{
	if (first->is_hostname == second->is_hostname)
		return pg_strcasecmp(first->string_value,
				     second->string_value) == 0;

	return od_address_equals((struct sockaddr *)&first->addr,
				 (struct sockaddr *)&second->addr) &&
	       od_address_equals((struct sockaddr *)&first->mask,
				 (struct sockaddr *)&second->mask);
}

static bool od_address_hostname_match(const char *pattern,
				      const char *actual_hostname)
{
	if (pattern[0] == '.') /* suffix match */
	{
		size_t plen = strlen(pattern);
		size_t hlen = strlen(actual_hostname);
		if (hlen < plen)
			return false;
		return pg_strcasecmp(pattern,
				     actual_hostname + (hlen - plen)) == 0;
	} else
		return pg_strcasecmp(pattern, actual_hostname) == 0;
}

/*
 * Check to see if a connecting IP matches a given host name.
 */
static bool od_address_check_hostname(struct sockaddr_storage *client_sa,
				      const char *hostname)
{
	struct addrinfo *gai_result, *gai;
	int ret;
	bool found;

	char client_hostname[NI_MAXHOST];

	ret = getnameinfo((const struct sockaddr *)client_sa,
			  sizeof(*client_sa), client_hostname,
			  sizeof(client_hostname), NULL, 0, NI_NAMEREQD);

	if (ret != 0)
		return false;

	/* Now see if remote host name matches this pg_hba line */
	if (!od_address_hostname_match(hostname, client_hostname))
		return false;

	/* Lookup IP from host name and check against original IP */
	ret = getaddrinfo(client_hostname, NULL, NULL, &gai_result);
	if (ret != 0)
		return false;

	found = false;
	for (gai = gai_result; gai; gai = gai->ai_next) {
		found = od_address_equals(gai->ai_addr,
					  (struct sockaddr *)client_sa);
		if (found) {
			break;
		}
	}

	if (gai_result)
		freeaddrinfo(gai_result);

	return found;
}

bool od_address_validate(const od_address_range_t *address_range,
			 struct sockaddr_storage *sa)
{
	if (address_range->is_hostname)
		return od_address_check_hostname(sa,
						 address_range->string_value);

	if (address_range->addr.ss_family != sa->ss_family)
		return false;

	if (sa->ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;
		struct sockaddr_in *addr =
			(struct sockaddr_in *)&address_range->addr;
		struct sockaddr_in *mask =
			(struct sockaddr_in *)&address_range->mask;
		in_addr_t client_addr = sin->sin_addr.s_addr;
		in_addr_t client_net = mask->sin_addr.s_addr & client_addr;
		return (client_net ^ addr->sin_addr.s_addr) == 0;
	} else if (sa->ss_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6 *)sa;
		struct sockaddr_in6 *addr =
			(struct sockaddr_in6 *)&address_range->addr;
		struct sockaddr_in6 *mask =
			(struct sockaddr_in6 *)&address_range->mask;
		for (int i = 0; i < 16; ++i) {
			uint8_t client_net_byte = mask->sin6_addr.s6_addr[i] &
						  sin->sin6_addr.s6_addr[i];
			if (client_net_byte ^ addr->sin6_addr.s6_addr[i]) {
				return false;
			}
		}
		return true;
	}

	return false;
}

int od_address_hostname_validate(od_config_reader_t *reader, char *hostname)
{
	int reti =
		regexec(&reader->rfc952_hostname_regex, hostname, 0, NULL, 0);

	if (reti == 0) {
		return 0;
	}

	return 1;
}

static inline int od_address_parse_host(char *host, od_address_t *address)
{
	address->host = strdup(host);
	if (address->host == NULL) {
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

static inline int od_address_parse_port(const char *port, od_address_t *address)
{
	if (address->port != 0) {
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

	address->port = val;

	return OK_RESPONSE;
}

static inline int od_address_parse_zone(const char *zone, od_address_t *address)
{
	if (strlen(address->availability_zone) != 0) {
		/* can not set twice */
		return NOT_OK_RESPONSE;
	}

	int len = strlen(zone);
	if (len > OD_MAX_AVAILABILITY_ZONE_LENGTH - 1 /* 0-byte */) {
		return NOT_OK_RESPONSE;
	}

	strcpy(address->availability_zone, zone);

	return OK_RESPONSE;
}

static const char *ADDRESS_TCP_PREFIX = "tcp://";
static const char *ADDRESS_UNIX_PREFIX = "unix://";

static inline bool od_address_parse_type_check_prefix(char **host,
						      const char *prefix)
{
	if (strncmp(prefix, *host, strlen(prefix)) == 0) {
		(*host) += strlen(prefix);
		return true;
	}

	return false;
}

static inline od_address_type_t od_address_parse_type(char **host)
{
	if (od_address_parse_type_check_prefix(host, ADDRESS_TCP_PREFIX)) {
		return OD_ADDRESS_TYPE_TCP;
	}

	if (od_address_parse_type_check_prefix(host, ADDRESS_UNIX_PREFIX)) {
		return OD_ADDRESS_TYPE_UNIX;
	}

	return OD_ADDRESS_TYPE_TCP;
}

static inline int od_address_parse(char *buff, od_address_t *address)
{
	char *strtok_preserve = NULL;
	char *token = NULL;

	address->type = od_address_parse_type(&buff);

	if (buff[0] != '[') {
		token = strtok_r(buff, ":", &strtok_preserve);
		if (token == NULL) {
			goto error;
		}

		if (od_address_parse_host(token, address) != OK_RESPONSE) {
			goto error;
		}

		token = strtok_r(NULL, ":", &strtok_preserve);
	} else {
		/* need to find ']' by ourself */
		char *host = buff + 1;
		char *end = strchr(host, ']');
		if (end == NULL) {
			goto error;
		}
		*end = 0;

		if (od_address_parse_host(host, address) != OK_RESPONSE) {
			goto error;
		}

		token = strtok_r(end + 1, ":", &strtok_preserve);
	}

	while (token != NULL) {
		if (strlen(token) == 0) {
			goto error;
		}

		if (isdigit(token[0])) {
			if (od_address_parse_port(token, address) !=
			    OK_RESPONSE) {
				goto error;
			}
		} else if (od_address_parse_zone(token, address) !=
			   OK_RESPONSE) {
			goto error;
		}

		token = strtok_r(NULL, ":", &strtok_preserve);
	}

	return OK_RESPONSE;

error:
	od_address_destroy(address);
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

int od_parse_addresses(const char *host_str, od_address_t **out, size_t *count)
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
	 * 
	 * tcp://localhost:1337
	 * unix:///var/lib/postgresql/.s.PGSQL.5432
	 * tcp://localhost:1337,unix:///var/lib/postgresql/.s.PGSQL.5432
	 */

	static __thread char buff[4096];
	char *strtok_preserve = NULL;

	int len = strlen(host_str);
	if (len > (int)sizeof(buff) - 1 /* 0-byte */) {
		return NOT_OK_RESPONSE;
	}
	strcpy(buff, host_str);

	size_t result_count = od_config_reader_get_endpoints_count(buff, len);
	od_address_t *result = od_malloc(result_count * sizeof(od_address_t));
	if (result == NULL) {
		return NOT_OK_RESPONSE;
	}
	od_address_t *address = result;

	char *next_address = strtok_r(buff, ",", &strtok_preserve);
	while (next_address != NULL) {
		od_address_init(address);

		if (od_address_parse(next_address, address) != OK_RESPONSE) {
			/* destroy already created addresses */
			od_address_t *addr_to_free = result;
			while (addr_to_free != address) {
				od_address_destroy(addr_to_free);
				++addr_to_free;
			}

			od_free(result);

			return NOT_OK_RESPONSE;
		}

		++address;
		next_address = strtok_r(NULL, ",", &strtok_preserve);
	}

	*out = result;
	*count = result_count;

	return OK_RESPONSE;
}

void od_address_init(od_address_t *addr)
{
	memset(addr, 0, sizeof(od_address_t));
}

void od_address_move(od_address_t *dst, od_address_t *src)
{
	od_address_destroy(dst);

	memcpy(dst, src, sizeof(od_address_t));

	od_address_init(src);
}

int od_address_copy(od_address_t *dst, const od_address_t *src)
{
	od_address_destroy(dst);

	memcpy(dst, src, sizeof(od_address_t));

	dst->host = strdup(src->host);
	if (dst->host == NULL) {
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

void od_address_destroy(od_address_t *addr)
{
	od_free(addr->host);
}

static inline int od_address_unix_cmp(const od_address_t *a,
				      const od_address_t *b)
{
	assert(a->type == OD_ADDRESS_TYPE_UNIX);
	assert(b->type == OD_ADDRESS_TYPE_UNIX);

	return strcmp(a->host, b->host);
}

static inline int od_address_tcp_cmp(const od_address_t *a,
				     const od_address_t *b)
{
	assert(a->type == OD_ADDRESS_TYPE_TCP);
	assert(b->type == OD_ADDRESS_TYPE_TCP);

	if (a->port != b->port) {
		return a->port - b->port;
	}

	int zone_cmp = strcmp(a->availability_zone, b->availability_zone);
	if (zone_cmp != 0) {
		return zone_cmp;
	}

	return strcmp(a->host, b->host);
}

int od_address_cmp(const od_address_t *a, const od_address_t *b)
{
	if (a->type != b->type) {
		return a->type - b->type;
	}

	if (a->type == OD_ADDRESS_TYPE_UNIX) {
		return od_address_unix_cmp(a, b);
	}

	if (a->type == OD_ADDRESS_TYPE_TCP) {
		return od_address_tcp_cmp(a, b);
	}

	abort();
}

void od_address_to_str(const od_address_t *addr, char *out, size_t max)
{
	if (addr->type == OD_ADDRESS_TYPE_UNIX) {
		od_snprintf(out, max, "unix://%s", addr->host);
		return;
	}

	if (addr->type == OD_ADDRESS_TYPE_TCP) {
		od_snprintf(out, max, "tcp://%s:%d", addr->host, addr->port);
		return;
	}

	abort();
}

int od_address_is_localhost(const od_address_t *addr)
{
	if (addr->type == OD_ADDRESS_TYPE_UNIX) {
		return 1;
	}

	if (addr->type == OD_ADDRESS_TYPE_TCP) {
		/* TODO: maybe use gethostbyname here */
		if (strcmp(addr->host, "localhost") == 0) {
			return 1;
		}

		if (strcmp(addr->host, "127.0.0.1") == 0) {
			return 1;
		}

		if (strcmp(addr->host, "::1") == 0) {
			return 1;
		}

		return 0;
	}

	abort();
}

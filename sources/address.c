#include <machinarium.h>
#include <odyssey.h>
#include <regex.h>

/*
* Odyssey.
*
* Scalable PostgreSQL connection pooler.
*/

od_address_range_t od_address_range_create_default()
{
	od_address_range_t address_range = { .string_value = strdup("all"),
					     .string_value_len = strlen("all"),
					     .is_default = 1 };
	return address_range;
}

void od_address_range_copy(od_address_range_t *src, od_address_range_t *dst)
{
	dst->string_value = strndup(src->string_value, src->string_value_len);
	dst->string_value_len = src->string_value_len;
	dst->addr = src->addr;
	dst->mask = src->mask;
	dst->is_default = src->is_default;
	dst->is_hostname = src->is_hostname;
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
		uint32 mask;
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

bool od_address_range_equals(od_address_range_t *first,
			     od_address_range_t *second)
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
		return (pg_strcasecmp(pattern,
				      actual_hostname + (hlen - plen)) == 0);
	} else
		return (pg_strcasecmp(pattern, actual_hostname) == 0);
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

	ret = getnameinfo(client_sa, sizeof(*client_sa), client_hostname,
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

bool od_address_validate(od_address_range_t *address_range,
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

int od_address_hostname_validate(char *hostname)
{
	regex_t regex;
	char *valid_rfc952_hostname_regex =
		"^(\\.?(([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z]|[A-Za-z][A-Za-z0-9\\-]*[A-Za-z0-9]))$";
	int reti = regcomp(&regex, valid_rfc952_hostname_regex, REG_EXTENDED);
	if (reti)
		return -1;
	reti = regexec(&regex, hostname, 0, NULL, 0);
	if (reti == 0)
		return 0;
	else
		return 1;
}

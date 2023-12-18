
/*
* Odyssey.
*
* Scalable PostgreSQL connection pooler.
*/

#include <machinarium.h>
#include <odyssey.h>
#include <regex.h>

od_address_range_t od_address_range_create_default()
{
	od_address_range_t address_range = {
		.string = strdup("all"),
		.string_len = strlen("all"),
		.is_default = 1
	};
	return address_range;
}

int od_address_range_copy(od_address_range_t *src, od_address_range_t *dst)
{
	dst->string = strndup(src->string, src->string_len);
	dst->string_len = src->string_len;
	dst->addr = src->addr;
	dst->mask = src->mask;
}

int od_address_range_read_prefix(od_address_range_t *address_range, char *prefix)
{
	char *end = NULL;
	long len = strtoul(prefix, &end, 10);
	if (*prefix == '\0' || *end != '\0') {
		return -1;
	}
	if (address_range->addr.ss_family == AF_INET) {
		if (len > 32)
			return -1;
		struct sockaddr_in *addr = (struct sockaddr_in *)&address_range->mask;
		long mask;
		if (len > 0)
			mask = (0xffffffffUL << (32 - (int)len)) & 0xffffffffUL;
		else
			mask = 0;
		addr->sin_addr.s_addr = od_address_bswap32(mask);
		return 0;
	} else if (address_range->addr.ss_family == AF_INET6) {
		if (len > 128)
			return -1;
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&address_range->mask;
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

bool od_address_equals(struct sockaddr_storage *firstAddress,
		       struct sockaddr_storage *secondAddress)
{
	if (firstAddress->ss_family != secondAddress->ss_family)
		return false;

	if (firstAddress->ss_family == AF_INET) {
		struct sockaddr_in *addr1 = (struct sockaddr_in *)firstAddress;
		struct sockaddr_in *addr2 = (struct sockaddr_in *)secondAddress;
		return (addr1->sin_addr.s_addr ^ addr2->sin_addr.s_addr) == 0;
	} else if (firstAddress->ss_family == AF_INET6) {
		struct sockaddr_in6 *addr1 = (struct sockaddr_in6 *)firstAddress;
		struct sockaddr_in6 *addr2 = (struct sockaddr_in6 *)secondAddress;
		for (int i = 0; i < 16; ++i) {
			if (addr1->sin6_addr.s6_addr[i] ^ addr2->sin6_addr.s6_addr[i]) {
				return false;
			}
		}
		return true;
	} else if (firstAddress->ss_family == AF_UNSPEC) {
		return true;
	}

	return false;
}

bool od_address_range_equals(od_address_range_t *first, od_address_range_t *second)
{
	return od_address_equals(&first->addr, &second->addr) &&
	       od_address_equals(&first->mask, &second->mask);
}

bool od_address_validate(od_address_range_t *address_range, struct sockaddr_storage *sa)
{
	if (address_range->addr.ss_family != sa->ss_family)
		return false;

	if (sa->ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;
		struct sockaddr_in *addr = (struct sockaddr_in *)&address_range->addr;
		struct sockaddr_in *mask = (struct sockaddr_in *)&address_range->mask;
		in_addr_t client_addr = sin->sin_addr.s_addr;
		in_addr_t client_net = mask->sin_addr.s_addr & client_addr;
		return (client_net ^ addr->sin_addr.s_addr) == 0;
	} else if (sa->ss_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6 *)sa;
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&address_range->addr;
		struct sockaddr_in6 *mask = (struct sockaddr_in6 *)&address_range->mask;
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

bool od_address_hostname_validate(char *hostname)
{
	regex_t regex;
	char *valid_rfc952_hostname_regex = "^(([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z]|[A-Za-z][A-Za-z0-9\\-]*[A-Za-z0-9])$";
	return regcomp(&regex, valid_rfc952_hostname_regex, 0) == 0;
}

uint32 od_address_bswap32(uint32 x)
{
	return ((x << 24) & 0xff000000) | ((x << 8) & 0x00ff0000) |
	       ((x >> 8) & 0x0000ff00) | ((x >> 24) & 0x000000ff);
}

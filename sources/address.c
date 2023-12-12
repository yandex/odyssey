
/*
* Odyssey.
*
* Scalable PostgreSQL connection pooler.
*/

#include <machinarium.h>
#include <odyssey.h>

int od_address_read_prefix(od_address_range_t *address_range, char *prefix)
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

bool od_address_inet_equals(struct sockaddr_storage *firstAddress,
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
	}
	return false;
}

uint32 od_address_bswap32(uint32 x)
{
	return ((x << 24) & 0xff000000) | ((x << 8) & 0x00ff0000) |
	       ((x >> 8) & 0x0000ff00) | ((x >> 24) & 0x000000ff);
}


/*
* Odyssey.
*
* Scalable PostgreSQL connection pooler.
*/

#include <machinarium.h>
#include <odyssey.h>
#include <regex.h>

#define HIGHBIT (0x80)
#define IS_HIGHBIT_SET(ch) ((unsigned char)(ch) & HIGHBIT)

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

bool od_address_equals(struct sockaddr_storage *firstAddress,
		       struct sockaddr_storage *secondAddress)
{
	if (firstAddress->ss_family != secondAddress->ss_family)
		return false;

	if (firstAddress->ss_family == AF_INET) {
		return od_address_ipv4eq((struct sockaddr_in *)firstAddress,
					 (struct sockaddr_in *)secondAddress);
	} else if (firstAddress->ss_family == AF_INET6) {
		return od_address_ipv6eq((struct sockaddr_in6 *)firstAddress,
					(struct sockaddr_in6 *)secondAddress);
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
	if (address_range->is_hostname)
		return od_address_check_hostname(sa, address_range->string);

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

int od_address_hostname_validate(char *hostname)
{
	regex_t regex;
	char *valid_rfc952_hostname_regex = "^(([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z]|[A-Za-z][A-Za-z0-9\\-]*[A-Za-z0-9])$";
	int reti = regcomp(&regex, valid_rfc952_hostname_regex, REG_EXTENDED);
	if (reti)
		return -1;
	reti = regexec(&regex, hostname, 0, NULL, 0);
	if (reti == 0)
		return 0;
	else
		return 1;
}

uint32 od_address_bswap32(uint32 x)
{
	return ((x << 24) & 0xff000000) | ((x << 8) & 0x00ff0000) |
	       ((x >> 8) & 0x0000ff00) | ((x >> 24) & 0x000000ff);
}

static bool od_address_hostname_match(const char *pattern, const char *actual_hostname)
{
	if (pattern[0] == '.')		/* suffix match */
	{
		size_t	plen = strlen(pattern);
		size_t	hlen = strlen(actual_hostname);

		if (hlen < plen)
			return false;

		return (od_address_strcasecmp(pattern, actual_hostname + (hlen - plen)) == 0);
	}
	else
		return (od_address_strcasecmp(pattern, actual_hostname) == 0);
}

static int od_address_strcasecmp(const char *s1, const char *s2)
{
	for (;;)
	{
		unsigned char ch1 = (unsigned char) *s1++;
		unsigned char ch2 = (unsigned char) *s2++;

		if (ch1 != ch2)
		{
			if (ch1 >= 'A' && ch1 <= 'Z')
				ch1 += 'a' - 'A';
			else if (IS_HIGHBIT_SET(ch1) && isupper(ch1))
				ch1 = tolower(ch1);

			if (ch2 >= 'A' && ch2 <= 'Z')
				ch2 += 'a' - 'A';
			else if (IS_HIGHBIT_SET(ch2) && isupper(ch2))
				ch2 = tolower(ch2);

			if (ch1 != ch2)
				return (int) ch1 - (int) ch2;
		}
		if (ch1 == 0)
			break;
	}
	return 0;
}

/*
 * Check to see if a connecting IP matches a given host name.
 */
static bool od_address_check_hostname(struct sockaddr_storage *client_sa, const char *hostname)
{
	struct addrinfo *gai_result, *gai;
	int ret;
	bool found;

	char client_hostname[NI_MAXHOST];

	ret = getnameinfo(client_sa, sizeof(*client_sa),
			  client_hostname, sizeof(client_hostname),
			  NULL, 0,
			  NI_NAMEREQD);

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
	for (gai = gai_result; gai; gai = gai->ai_next)
	{
		if (gai->ai_addr->sa_family == client_sa->ss_family)
		{
			if (gai->ai_addr->sa_family == AF_INET)
			{
				if (od_address_ipv4eq((struct sockaddr_in *)gai->ai_addr,
					              (struct sockaddr_in *)client_sa))
				{
					found = true;
					break;
				}
			}
			else if (gai->ai_addr->sa_family == AF_INET6)
			{
				if (od_address_ipv6eq((struct sockaddr_in6 *)gai->ai_addr,
					              (struct sockaddr_in6 *)&client_sa))
				{
					found = true;
					break;
				}
			}
		}
	}

	if (gai_result)
		freeaddrinfo(gai_result);

	return found;
}

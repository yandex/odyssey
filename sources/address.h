#ifndef ODYSSEY_ADDRESS_H
#define ODYSSEY_ADDRESS_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_address_read_prefix(struct sockaddr_storage *,
			   struct sockaddr_storage *, char *);

int od_address_read(struct sockaddr_storage *, const char *);

bool od_address_validate(od_rule_t *, struct sockaddr_storage *);

bool od_address_inet_compare(struct sockaddr_storage *,
			     struct sockaddr_storage *);

inline uint32 od_address_bswap32(uint32 x)
{
	return ((x << 24) & 0xff000000) | ((x << 8) & 0x00ff0000) |
	       ((x >> 8) & 0x0000ff00) | ((x >> 24) & 0x000000ff);
}

#endif /* ODYSSEY_ADDRESS_H */
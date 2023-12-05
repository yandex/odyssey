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

bool od_address_inet_compare(struct sockaddr_storage *,
			     struct sockaddr_storage *);

char od_address_string_convert(struct sockaddr_storage *);

uint32 od_address_bswap32(uint32);

#endif /* ODYSSEY_ADDRESS_H */
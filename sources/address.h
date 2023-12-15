#ifndef ODYSSEY_ADDRESS_H
#define ODYSSEY_ADDRESS_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef struct od_address_range od_address_range_t;

struct od_address_range {
	char *string;
	int string_len;
	struct sockaddr_storage addr;
	struct sockaddr_storage mask;
	int is_default;
};

od_address_range_t od_address_create_default();

int od_address_read_prefix(od_address_range_t *, char *);

int od_address_read(struct sockaddr_storage *, const char *);

bool od_address_inet_equals(struct sockaddr_storage *,
			    struct sockaddr_storage *);

bool od_address_validate(od_address_range_t *, struct sockaddr_storage *);

uint32 od_address_bswap32(uint32);

#endif /* ODYSSEY_ADDRESS_H */
#ifndef ODYSSEY_ADDRESS_H
#define ODYSSEY_ADDRESS_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef struct od_address_range od_address_range_t;

struct od_address_range {
	char *string_value;
	int string_value_len;
	struct sockaddr_storage addr;
	struct sockaddr_storage mask;
	int is_hostname;
	int is_default;
};

od_address_range_t od_address_range_create_default();

void od_address_range_copy(od_address_range_t *, od_address_range_t *);

int od_address_range_read_prefix(od_address_range_t *, char *);

int od_address_read(struct sockaddr_storage *, const char *);

bool od_address_equals(struct sockaddr *, struct sockaddr *);

bool od_address_range_equals(od_address_range_t *, od_address_range_t *);

bool od_address_validate(od_address_range_t *, struct sockaddr_storage *);

int od_address_hostname_validate(char *);

#endif /* ODYSSEY_ADDRESS_H */
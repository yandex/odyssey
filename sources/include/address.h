#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <sys/socket.h>

#include <common_const.h>
#include <types.h>

typedef enum { OD_ADDRESS_TYPE_UNIX, OD_ADDRESS_TYPE_TCP } od_address_type_t;

static inline const char *od_address_type_str(od_address_type_t type)
{
	switch (type) {
	case OD_ADDRESS_TYPE_TCP:
		return "tcp";
	case OD_ADDRESS_TYPE_UNIX:
		return "unix";
	default:
		abort();
	}
}

struct od_address {
	char *host; /* NULL - terminated */
	int port;
	od_address_type_t type;
	char availability_zone[OD_MAX_AVAILABILITY_ZONE_LENGTH];
};

void od_address_init(od_address_t *addr);
void od_address_move(od_address_t *dst, od_address_t *src);
int od_address_copy(od_address_t *dst, const od_address_t *src);
void od_address_destroy(od_address_t *addr);
int od_address_cmp(const od_address_t *a, const od_address_t *b);
int od_parse_addresses(const char *host_str, od_address_t **out, size_t *count);
void od_address_to_str(const od_address_t *addr, char *out, size_t max);
int od_address_is_localhost(const od_address_t *addr);

typedef struct od_address_range od_address_range_t;

struct od_address_range {
	char *string_value;
	int string_value_len;
	struct sockaddr_storage addr;
	struct sockaddr_storage mask;
	int is_hostname;
	int is_default;
};

od_address_range_t od_address_range_create_default(void);

void od_address_range_destroy(od_address_range_t *range);

int od_address_range_copy(const od_address_range_t *, od_address_range_t *);

int od_address_range_read_prefix(od_address_range_t *, char *);

int od_address_read(struct sockaddr_storage *, const char *);

bool od_address_equals(struct sockaddr *, struct sockaddr *);

bool od_address_range_equals(const od_address_range_t *,
			     const od_address_range_t *);

bool od_address_validate(const od_address_range_t *, struct sockaddr_storage *);

int od_address_hostname_validate(od_config_reader_t *, char *);

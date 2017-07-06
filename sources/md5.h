#ifndef SHAPITO_MD5_H
#define SHAPITO_MD5_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_md5 shapito_md5_t;

struct shapito_md5
{
	uint32_t state[4];
	uint64_t count;
	uint8_t  buffer[64];
};

void shapito_md5_init(shapito_md5_t*);
void shapito_md5_update(shapito_md5_t*, void*, size_t);
void shapito_md5_final(shapito_md5_t*, uint8_t[16]);
void shapito_md5_tostring(char*, uint8_t[16]);

#endif /* SHAPITO_MD5_H */

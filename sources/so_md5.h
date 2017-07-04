#ifndef SO_MD5_H_
#define SO_MD5_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_md5 so_md5_t;

struct so_md5
{
	uint32_t state[4];
	uint64_t count;
	uint8_t  buffer[64];
};

void so_md5_init(so_md5_t*);
void so_md5_update(so_md5_t*, void*, size_t);
void so_md5_final(so_md5_t*, uint8_t[16]);
void so_md5_tostring(char*, uint8_t[16]);

#endif

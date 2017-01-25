#ifndef SO_MD5_H_
#define SO_MD5_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_md5_t so_md5_t;

struct so_md5_t {
	uint32_t state[4];
	uint64_t count;
	uint8_t  buffer[64];
};

void so_md5_init(so_md5_t*);
void so_md5_update(so_md5_t*, void*, size_t);
void so_md5_final(so_md5_t*, uint8_t[16]);
void so_md5_tostring(uint8_t*, uint8_t[16]);

#endif

#ifndef SO_BEREAD_H_
#define SO_BEREAD_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct sobestartup_t sobestartup_t;

struct sobestartup_t {
	int      is_cancel;
	uint32_t key;
	uint32_t key_pid;
};

void so_bestartup_init(sobestartup_t*);
void so_bestartup_free(sobestartup_t*);

int so_beread_startup(sobestartup_t*, uint8_t*, uint32_t);

#endif

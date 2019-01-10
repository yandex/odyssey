
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

__thread unsigned short prng_seed[3];
__thread unsigned short *prng_state = NULL;

long int pg_lrand48(unsigned short *_rand48_seed);
void pg_srand48(long seed, unsigned short *_rand48_seed);

static void
od_id_mgr_seed()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	long int rand_seed_2 = 0;
	long int rand_seed;
	rand_seed = getpid() ^ getuid() ^ tv.tv_sec ^ tv.tv_usec;

	int fd;
	fd = open("/dev/random", O_RDONLY);
	if (fd == -1)
		fd = open("/dev/urandom", O_RDONLY);
	if (fd != -1) {
		int rc = read(fd, &rand_seed_2, sizeof(rand_seed_2));
		if (!rc)
		    close(fd);
	}

	rand_seed ^= rand_seed_2;
	pg_srand48(rand_seed, prng_seed);
	prng_state = prng_seed;
}

void
od_id_generate(od_id_t *id, char *prefix)
{
	if (!prng_state)
		od_id_mgr_seed();
	long int a = pg_lrand48(prng_state);
	long int b = pg_lrand48(prng_state);

	char seed[OD_ID_SEEDMAX];
	memcpy(seed + 0, &a, 4);
	memcpy(seed + 4, &b, 2);

	id->id_prefix = prefix;
	id->id_a = a;
	id->id_b = b;

	static const char *hex = "0123456789abcdef";
	int q, w;
	for (q = 0, w = 0; q < OD_ID_SEEDMAX; q++) {
		id->id[w++] = hex[(seed[q] >> 4) & 0x0F];
		id->id[w++] = hex[(seed[q]     ) & 0x0F];
	}
	assert(w == (OD_ID_SEEDMAX * 2));
}

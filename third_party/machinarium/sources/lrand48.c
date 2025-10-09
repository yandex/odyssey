/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

/* raw copy postgres rand48 impl for better compatibility */

#define RAND48_SEED_0 (0x330e)
#define RAND48_SEED_1 (0xabcd)
#define RAND48_SEED_2 (0x1234)
#define RAND48_MULT_0 (0xe66d)
#define RAND48_MULT_1 (0xdeec)
#define RAND48_MULT_2 (0x0005)
#define RAND48_ADD (0x000b)

static unsigned short _rand48_mult[3] = { RAND48_MULT_0, RAND48_MULT_1,
					  RAND48_MULT_2 };
static unsigned short _rand48_add = RAND48_ADD;

static inline void _dorand48(unsigned short xseed[3])
{
	unsigned long accu;
	unsigned short temp[2];

	accu = (unsigned long)_rand48_mult[0] * (unsigned long)xseed[0] +
	       (unsigned long)_rand48_add;
	temp[0] = (unsigned short)accu; /* lower 16 bits */
	accu >>= sizeof(unsigned short) * 8;
	accu += (unsigned long)_rand48_mult[0] * (unsigned long)xseed[1] +
		(unsigned long)_rand48_mult[1] * (unsigned long)xseed[0];
	temp[1] = (unsigned short)accu; /* middle 16 bits */
	accu >>= sizeof(unsigned short) * 8;
	accu += (long)_rand48_mult[0] * xseed[2] +
		(long)_rand48_mult[1] * xseed[1] +
		(long)_rand48_mult[2] * xseed[0];
	xseed[0] = temp[0];
	xseed[1] = temp[1];
	xseed[2] = (unsigned short)accu;
}

static inline double pg_impl_erand48(unsigned short xseed[3])
{
	_dorand48(xseed);
	return ldexp((double)xseed[0], -48) + ldexp((double)xseed[1], -32) +
	       ldexp((double)xseed[2], -16);
}

static inline long int pg_impl_lrand48(unsigned short *_rand48_seed)
{
	_dorand48(_rand48_seed);
	return ((long)_rand48_seed[2] << 15) + ((long)_rand48_seed[1] >> 1);
}

static inline void pg_impl_srand48(long seed, unsigned short *_rand48_seed)
{
	_rand48_seed[0] = RAND48_SEED_0;
	_rand48_seed[1] = (unsigned short)seed;
	_rand48_seed[2] = (unsigned short)(seed >> 16);
}

MM_THREAD_LOCAL unsigned short prng_seed[3];
MM_THREAD_LOCAL unsigned short *prng_state = NULL;

void mm_lrand48_seed(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	long int rand_seed_2 = 0;
	long int rand_seed;
	rand_seed = getpid() ^ getuid() ^ tv.tv_sec ^ tv.tv_usec;

	int fd;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		fd = open("/dev/random", O_RDONLY);
	if (fd != -1) {
		int rc = read(fd, &rand_seed_2, sizeof(rand_seed_2));
		(void)rc;
		close(fd);
	}

	rand_seed ^= rand_seed_2;
	pg_impl_srand48(rand_seed, prng_seed);
	prng_state = prng_seed;
}

MACHINE_API void machine_lrand48_seed(void)
{
	mm_lrand48_seed();
}

MACHINE_API long int machine_lrand48(void)
{
	assert(prng_state);
	return pg_impl_lrand48(prng_state);
}

MACHINE_API double machine_erand48(unsigned short xseed[3])
{
	return pg_impl_erand48(xseed);
}

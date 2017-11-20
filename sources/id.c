
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/atomic.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"

void od_idmgr_init(od_idmgr_t *mgr)
{
	memset(mgr->seed, 0, sizeof(mgr->seed));
	mgr->rand_state = 0;
	mgr->seq = 0;
	mgr->uid = getuid();
	mgr->pid = getpid();
}

int od_idmgr_seed(od_idmgr_t *mgr)
{
	struct timeval	tv;
	gettimeofday(&tv, 0);
	mgr->rand_state = (mgr->pid << 16) ^ mgr->uid ^ tv.tv_sec ^ tv.tv_usec;
	int i = 0;
	for (; i < OD_ID_SEEDMAX; i++)
		mgr->seed[i] = (rand_r(&mgr->rand_state) >> 7) & 0xFF;
	int fd;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		fd = open("/dev/random", O_RDONLY);
	if (fd == -1)
		return -1;
	char seed[OD_ID_SEEDMAX];
	int seed_read = 0;
	while (seed_read <= OD_ID_SEEDMAX) {
		int rc;
		rc = read(fd, seed, sizeof(seed));
		if (rc == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			close(fd);
			return -1;
		}
		seed_read += rc;
	}
	for (i = 0; i < OD_ID_SEEDMAX; i++)
		mgr->seed[i] ^= seed[i];
	close(fd);
	return 0;
}

void od_idmgr_generate(od_idmgr_t *mgr, od_id_t *id, char *prefix)
{
	id->id_prefix = prefix;

	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);

	time_t current_time = t.tv_sec;

	uint8_t second;
	second = current_time % 60;
	current_time /= 60;

	uint8_t minute;
	minute = current_time % 60;
	current_time /= 60;

	uint8_t hour;
	hour = current_time % 24;
	current_time /= 60;

	uint8_t day;
	day = current_time % 30;
	current_time /= 60;

	uint8_t month;
	month = current_time % 12;
	current_time /= 60;

	uint32_t seq;
	seq = ++mgr->seq;
	seq ^= t.tv_nsec;
	seq ^= machine_self();
	seq ^= rand_r(&mgr->rand_state);

	mgr->seed[0] ^= seq ^ second;
	mgr->seed[1] ^= seq ^ minute;
	mgr->seed[2] ^= seq ^ (hour + day + month);
	mgr->seed[3] ^= ((uint8_t*)&seq)[0] ^ ((uint8_t*)&seq)[1] ^ mgr->pid;
	mgr->seed[4] ^= ((uint8_t*)&seq)[2] ^ ((uint8_t*)&seq)[3] ^ mgr->uid;
	mgr->seed[5] ^= seq ^ (uintptr_t)id;

	char *dest = id->id;
	static const char *hex = "0123456789abcdef";
	int q, w;
	for (q = 0, w = 0; q < OD_ID_SEEDMAX; q++) {
		dest[w++] = hex[(mgr->seed[q] >> 4) & 0x0F];
		dest[w++] = hex[(mgr->seed[q]     ) & 0x0F];
	}
	assert(w == (OD_ID_SEEDMAX * 2));
}

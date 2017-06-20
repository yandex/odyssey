
/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <time.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <machinarium.h>
#include <shapito.h>

#include "od_macro.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_id.h"

void od_idmgr_init(od_idmgr_t *mgr)
{
	memset(mgr->seed, 0, sizeof(mgr->seed));
	mgr->seq = 0;
}

int od_idmgr_seed(od_idmgr_t *mgr, od_log_t *log)
{
	mgr->uid = getuid();
	mgr->pid = getpid();

	struct timeval	tv;
	gettimeofday(&tv, 0);
	srand((mgr->pid << 16) ^ mgr->uid ^ tv.tv_sec ^ tv.tv_usec);
	int i = 0;
	for (; i < 8; i++)
		mgr->seed[i] = (rand() >> 7) & 0xFF;
	int fd;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1)
		fd = open("/dev/random", O_RDONLY);
	if (fd == -1) {
		od_error(log, "id_manager", "failed to open /dev/{u}random");
		return -1;
	}
	char seed[8];
	int seed_read = 0;
	while (seed_read <= 8) {
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
	for (i = 0; i < 8; i++)
		mgr->seed[i] ^= seed[i];
	close(fd);
	return 0;
}

static const uint8_t codetable[] =
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwx";
static const size_t  codetable_size = sizeof(codetable);
static const size_t  codetable_single_divisor = sizeof(codetable) * 1;
static const size_t  codetable_double_divisor = sizeof(codetable) * 2;
static const size_t  codetable_triple_divisor = sizeof(codetable) * 3;

void od_idmgr_generate(od_idmgr_t *mgr, od_id_t *id)
{
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

	uint64_t seq;
	seq = ++mgr->seq;
	seq ^= t.tv_nsec;

	uint8_t *seed = &id->id[0];
	seed[0] = mgr->seed[0] ^ codetable[second];
	seed[1] = mgr->seed[1] ^ codetable[minute];
	seed[2] = mgr->seed[2] ^ codetable[(hour + day + month) % codetable_size];
	seed[3] = mgr->seed[3] ^ codetable[(seq) % codetable_size];
	seed[4] = mgr->seed[4] ^ codetable[(seq / codetable_single_divisor) % codetable_size];
	seed[5] = mgr->seed[5] ^ codetable[(seq / codetable_double_divisor) % codetable_size];
	seed[6] = mgr->seed[6] ^ codetable[(seq / codetable_triple_divisor) % codetable_size];
	seed[7] = mgr->seed[7] ^ codetable[(uintptr_t)id % codetable_size];
	seed[8] = mgr->seed[8] ^ codetable[(mgr->pid + mgr->uid) % codetable_size];
	memcpy(mgr->seed, seed, 8);
}

#ifndef OD_STAT_H_
#define OD_STAT_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_stat_t od_stat_t;

#define OD_STAT_MAX 8

struct od_stat_t {
	int pos;
	uint32_t stat[OD_STAT_MAX];
};

static inline void
od_stat_init(od_stat_t *stat)
{
	stat->pos = 0;
	memset(stat->stat, 0, sizeof(stat->stat));
}

static inline void
od_stat_tick(od_stat_t *stat)
{
	stat->pos = (stat->pos + 1) % OD_STAT_MAX;
	stat->stat[stat->pos] = 0;
}

static inline void
od_stat_add(od_stat_t *stat, uint32_t value)
{
	stat->stat[stat->pos] += value;
}

static inline float
od_stat_avg(od_stat_t *stat)
{
	float sum = 0;
	int i = 0;
	while (i < OD_STAT_MAX) {
		sum += stat->stat[i];
		i++;
	}
	return sum / (float)OD_STAT_MAX;
}

#endif

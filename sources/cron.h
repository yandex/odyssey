#ifndef OD_CRON_H
#define OD_CRON_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_cron od_cron_t;

struct od_cron
{
	od_system_t *system;
};

void od_cron_init(od_cron_t*, od_system_t*);
int  od_cron_start(od_cron_t*);

#endif /* OD_CRON_H */

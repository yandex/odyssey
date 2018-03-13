#ifndef OD_CRON_H
#define OD_CRON_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_cron od_cron_t;

struct od_cron
{
	od_global_t *global;
};

void od_cron_init(od_cron_t*, od_global_t*);
int  od_cron_start(od_cron_t*);

#endif /* OD_CRON_H */

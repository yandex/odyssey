#ifndef OD_GLOBAL_H
#define OD_GLOBAL_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_global od_global_t;

struct od_global
{
	void *instance;
	void *system;
	void *router;
	void *console;
	void *cron;
	void *worker_pool;
};

#endif /* OD_GLOBAL_H */

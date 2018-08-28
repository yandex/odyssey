#ifndef ODYSSEY_GLOBAL_H
#define ODYSSEY_GLOBAL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
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

#endif /* ODYSSEY_GLOBAL_H */

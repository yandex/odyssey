#ifndef OD_SYSTEM_H
#define OD_SYSTEM_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_system od_system_t;

struct od_system
{
	void *instance;
	void *pooler;
	void *router;
	void *console;
	void *periodic;
	void *worker_pool;
};

#endif /* OD_SYSTEM_H */

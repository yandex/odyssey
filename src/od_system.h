#ifndef OD_SYSTEM_H
#define OD_SYSTEM_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_system od_system_t;

struct od_system
{
	void *instance;
	void *pooler;
	void *router;
	void *relay_pool;
	machine_queue_t task_queue;
};

#endif /* OD_SYSTEM_H */

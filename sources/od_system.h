#ifndef OD_SYSTEM_H
#define OD_SYSTEM_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_system od_system_t;

struct od_system
{
	void *instance;
	void *pooler;
	void *router;
	void *console;
	void *periodic;
	void *relay_pool;
};

#endif /* OD_SYSTEM_H */

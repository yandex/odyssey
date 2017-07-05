#ifndef OD_RELAY_H
#define OD_RELAY_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_relay od_relay_t;

struct od_relay
{
	int64_t          machine;
	int              id;
	machine_queue_t *task_queue;
	od_system_t     *system;
};

void od_relay_init(od_relay_t*, od_system_t*, int);
int  od_relay_start(od_relay_t*);

#endif /* OD_RELAY_H */

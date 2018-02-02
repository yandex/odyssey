#ifndef OD_PERIODIC_H
#define OD_PERIODIC_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_periodic_t od_periodic_t;

struct od_periodic_t {
	od_system_t *system;
};

void od_periodic_init(od_periodic_t*, od_system_t*);
int  od_periodic_start(od_periodic_t*);

#endif /* OD_PERIODIC_H */

#ifndef OD_PERIODIC_H
#define OD_PERIODIC_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_periodic_t od_periodic_t;

struct od_periodic_t {
	od_system_t *system;
};

int od_periodic_init(od_periodic_t*, od_system_t*);
int od_periodic_start(od_periodic_t*);

#endif /* OD_PERIODIC_H */

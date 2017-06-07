#ifndef OD_RELAY_H
#define OD_RELAY_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_relay od_relay_t;

struct od_relay
{
	int64_t      machine;
	int          id;
	od_system_t *system;
};

void od_relay_init(od_relay_t*, od_system_t*, int);
int  od_relay_start(od_relay_t*);

#endif /* OD_RELAY_H */

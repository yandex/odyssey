#ifndef OD_CONSOLE_H
#define OD_CONSOLE_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_console_t od_console_t;

typedef enum
{
	OD_COK,
	OD_CERROR
} od_consolestatus_t;

struct od_console_t {
	od_system_t *system;
	machine_queue_t *queue;
};

int od_console_init(od_console_t*, od_system_t*);
int od_console_start(od_console_t*);

od_consolestatus_t
od_console_request(od_client_t*, char*);

#endif /* OD_CONSOLE_H */

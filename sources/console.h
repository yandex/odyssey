#ifndef OD_CONSOLE_H
#define OD_CONSOLE_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_console_t od_console_t;

typedef enum
{
	OD_COK,
	OD_CERROR
} od_consolestatus_t;

struct od_console_t {
	od_system_t       *system;
	machine_channel_t *channel;
};

int od_console_init(od_console_t*, od_system_t*);
int od_console_start(od_console_t*);

od_consolestatus_t
od_console_request(od_client_t*, char*, int);

#endif /* OD_CONSOLE_H */

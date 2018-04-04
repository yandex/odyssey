#ifndef OD_CONSOLE_H
#define OD_CONSOLE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_console_t od_console_t;

typedef enum
{
	OD_COK,
	OD_CERROR
} od_consolestatus_t;

struct od_console_t {
	machine_channel_t *channel;
	od_global_t       *global;
};

void od_console_init(od_console_t*, od_global_t*);
int  od_console_start(od_console_t*);

od_consolestatus_t
od_console_request(od_client_t*, char*, int);

#endif /* OD_CONSOLE_H */

#ifndef ODYSSEY_CONSOLE_H
#define ODYSSEY_CONSOLE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_console od_console_t;

struct od_console
{
	machine_channel_t *channel;
	od_global_t       *global;
};

void od_console_init(od_console_t*, od_global_t*);
int  od_console_start(od_console_t*);
int  od_console_request(od_client_t*, machine_channel_t*, machine_msg_t*);

#endif /* ODYSSEY_CONSOLE_H */

#ifndef ODYSSEY_CONSOLE_H
#define ODYSSEY_CONSOLE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

int od_console_setup(od_client_t*);
int od_console_query(od_client_t*, machine_msg_t*);

#endif /* ODYSSEY_CONSOLE_H */

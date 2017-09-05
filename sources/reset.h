#ifndef OD_RESET_H
#define OD_RESET_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int od_reset(od_server_t*);
int od_reset_configure(od_server_t*, shapito_be_startup_t*);
int od_reset_discard(od_server_t*);

#endif /* OD_RESET_H */

#ifndef OD_RESET_H
#define OD_RESET_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int od_reset(od_server_t*);
int od_reset_configure(od_server_t*, char*, shapito_parameters_t*);
int od_reset_discard(od_server_t*, char*);

#endif /* OD_RESET_H */

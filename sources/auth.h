#ifndef OD_AUTH_H
#define OD_AUTH_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int od_auth_frontend(od_client_t*);
int od_auth_backend(od_server_t*);

#endif /* OD_AUTH_H */

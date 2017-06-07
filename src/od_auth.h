#ifndef OD_AUTH_H
#define OD_AUTH_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_auth_frontend(od_client_t*);
int od_auth_backend(od_server_t*);

#endif /* OD_AUTH_H */

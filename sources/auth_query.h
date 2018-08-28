#ifndef ODYSSEY_AUTH_QUERY_H
#define ODYSSEY_AUTH_QUERY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

int od_auth_query(od_global_t*, od_config_route_t*, kiwi_param_t*,
                  kiwi_password_t*);

#endif /* ODYSSEY_AUTH_QUERY_H */

#ifndef OD_AUTH_QUERY_H
#define OD_AUTH_QUERY_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int od_auth_query(od_system_t*, shapito_stream_t*, od_configroute_t*,
                  shapito_parameter_t*, shapito_password_t*);

#endif /* OD_AUTH_QUERY_H */

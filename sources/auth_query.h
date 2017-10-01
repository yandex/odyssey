#ifndef OD_AUTH_QUERY_H
#define OD_AUTH_QUERY_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int
od_auth_query(od_instance_t *instance, od_schemeroute_t *scheme,
              shapito_password_t *password);

#endif /* OD_AUTH_QUERY_H */

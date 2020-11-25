#ifndef ODYSSEY_AUTH_QUERY_H
#define ODYSSEY_AUTH_QUERY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "common_const.h"

int
od_auth_query(od_global_t *,
              od_rule_t *,
              char *,
              kiwi_var_t *,
              kiwi_password_t *);

#endif /* ODYSSEY_AUTH_QUERY_H */

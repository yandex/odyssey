#pragma once

#ifdef LDAP_FOUND

#include <kiwi/kiwi.h>

#include <types.h>
#include <id.h>
#include <server.h>
#include <list.h>
#include <ldap_endpoint.h>
#include <logger.h>
#include <thread_pool.h>

/* For functions ldap_unbind, ldap_search_s, ldap_simple_bind_s */
#define LDAP_DEPRECATED 1

#include <ldap.h>

int od_auth_ldap(od_client_t *cl, kiwi_password_t *tok);
int od_auth_ldap_resolve_storage_credentials(od_client_t *cl, od_rule_t *rule);

int od_ldap_workers_init(size_t count);
void od_ldap_workers_destroy(void);

#endif /* LDAP_FOUND */

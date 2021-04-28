#ifndef ODYSSEY_LDAP_H
#define ODYSSEY_LDAP_H

#include <ldap.h>

typedef struct {
	od_id_t id;

	LDAP *conn;
	// connect url
	od_ldap_endpoint_t *endpoint; // link to actual settings;
	od_server_state_t state;

	od_global_t *global;
	void *route;

	char *auth_user;

	od_list_t link;
} od_ldap_server_t;

extern od_retcode_t od_auth_ldap(od_client_t *cl, kiwi_password_t *tok);

extern od_retcode_t od_ldap_server_free(od_ldap_server_t *serv);
extern od_ldap_server_t *od_ldap_server_allocate();

#endif /* ODYSSEY_LDAP_H */

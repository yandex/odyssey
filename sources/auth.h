#ifndef ODYSSEY_AUTH_H
#define ODYSSEY_AUTH_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_auth_frontend(od_client_t *);
int od_auth_backend(od_server_t *, machine_msg_t *);

typedef od_retcode_t (*od_auth_cleartext_hook_type)(od_client_t *cl,
						    kiwi_password_t *tok);
extern od_auth_cleartext_hook_type od_auth_cleartext_hook;

#endif /* ODYSSEY_AUTH_H */

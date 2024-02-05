#ifndef ODYSSEY_IAMPROXY_H
#define ODYSSEY_IAMPROXY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int mdb_iamproxy_authenticate_user(const char *username, const char *token,
				   od_instance_t *instance,
				   od_client_t *client);

#endif /* ODYSSEY_IAMPROXy_H */

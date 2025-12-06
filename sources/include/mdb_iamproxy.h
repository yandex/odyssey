#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int mdb_iamproxy_authenticate_user(char *username, char *token,
				   od_instance_t *instance,
				   od_client_t *client);

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

int external_user_authentication(char *username, char *token,
				 od_instance_t *instance, od_client_t *client);

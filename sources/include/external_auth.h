#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

typedef enum {
	OD_EAUTH_ERROR = -1,
	OD_EAUTH_OK = 0,
	OD_EAUTH_DENIED = 1,
} od_external_auth_status_t;

od_external_auth_status_t external_user_authentication(char *username,
						       char *token,
						       od_instance_t *instance,
						       od_client_t *client);

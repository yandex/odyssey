/*
 * Odyssey module.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "audit_module.h"

od_module_t od_module = {
	.auth_attempt_cb = audit_auth_cb,
	.disconnect_cb   = audit_disconnect_cb,
	.unload_cb       = audit_auth_unload,
	.config_init_cb  = audit_config_init,
};

int
audit_auth_cb(od_client_t *c, bool auth_ok)
{
	FILE *fptr;
	fptr = fopen("/tmp/audit_usr.log", "a");

	if (fptr == NULL) {
		printf("Error!");
		return -1;
	}
	if (auth_ok) {
		fprintf(fptr,
		        "usr successfully logged in: usrname = %s",
		        c->startup.user.value);
	} else {
		fprintf(
		  fptr, "usr failed to log in: usrname = %s", c->startup.user.value);
	}

	fclose(fptr);
	return 0;
}

int
audit_disconnect_cb(od_client_t *c, od_status_t s)
{
	return 0;
}

int
audit_auth_unload()
{
	return 0;
}

int
audit_config_init(od_config_reader_t *cr)
{
	return 0;
}

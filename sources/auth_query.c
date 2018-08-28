
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

int
od_auth_query(od_global_t *global, od_config_route_t *config,
              kiwi_param_t *user,
              kiwi_password_t *password)
{
	(void)global;
	(void)config;
	(void)user;
	(void)password;
	return 0;
}

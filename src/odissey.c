
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <machinarium.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_instance.h"

int main(int argc, char *argv[])
{
	machinarium_init();
	od_instance_t odissey;
	od_instance_init(&odissey);
	int rc = od_instance_main(&odissey, argc, argv);
	od_instance_free(&odissey);
	machinarium_free();
	return rc;
}


/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <machinarium.h>

#include "sources/macro.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/syslog.h"
#include "sources/log.h"
#include "sources/scheme.h"
#include "sources/lex.h"
#include "sources/config.h"
#include "sources/instance.h"

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

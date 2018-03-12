
/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/config.h"
#include "sources/config_mgr.h"
#include "sources/config_reader.h"
#include "sources/instance.h"

int main(int argc, char *argv[])
{
	od_instance_t odyssey;
	od_instance_init(&odyssey);
	int rc = od_instance_main(&odyssey, argc, argv);
	if (rc == -1) {
		rc = EXIT_FAILURE;
	} else {
		rc = EXIT_SUCCESS;
	}
	od_instance_free(&odyssey);
	return rc;
}

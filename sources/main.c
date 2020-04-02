
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
main(int argc, char *argv[])
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

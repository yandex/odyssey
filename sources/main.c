
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	od_instance_t *odyssey = od_instance_create();
	odyssey->orig_argv_ptr = argv[0];

	int rc = od_instance_main(odyssey, argc, argv);
	if (rc == -1) {
		rc = EXIT_FAILURE;
	} else {
		rc = EXIT_SUCCESS;
	}

	return rc;
}

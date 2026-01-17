
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <instance.h>

int get_args_len_sum(int argc, char *argv[])
{
	/*
	 * we will use argv[0] as a way to set proc title
	 * and before write new title - previous content must be replace with
	 * 0, otherwise, there might be some trash after result title
	 */

	int sum = 0;

	for (int i = 0; i < argc; ++i) {
		sum += strlen(argv[i]) + 1 /* 0-byte */;
	}

	return sum;
}

int main(int argc, char *argv[], char *envp[])
{
	od_instance_t *odyssey = od_instance_create();
	odyssey->orig_argv_ptr = argv[0];
	odyssey->orig_argv_ptr_len = get_args_len_sum(argc, argv);

	int rc = od_instance_main(odyssey, argc, argv, envp);
	if (rc == -1) {
		rc = EXIT_FAILURE;
	} else {
		rc = EXIT_SUCCESS;
	}

	return rc;
}

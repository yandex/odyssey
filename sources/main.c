
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <instance.h>

#ifdef USE_TCMALLOC_PROFILE
#include <gperftools/heap-profiler.h>
#include <gperftools/tcmalloc.h>

static inline void init_tcmalloc_profile()
{
	const char *heapprofile_path = getenv("HEAPPROFILE");

	if (heapprofile_path == NULL) {
		return;
	}

	fprintf(stderr, "Starting heap profiler: %s\n", heapprofile_path);
	HeapProfilerStart(heapprofile_path);

	if (!IsHeapProfilerRunning()) {
		abort();
	}
}
#else
static inline void init_tcmalloc_profile()
{
}
#endif /* USE_TCMALLOC_PROFILE */

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
	init_tcmalloc_profile();

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

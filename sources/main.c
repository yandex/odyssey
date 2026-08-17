
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <instance.h>

#ifdef __darwin__
#include <crt_externs.h>
#endif

#ifdef USE_TCMALLOC_PROFILE
#include <gperftools/heap-profiler.h>
#include <gperftools/tcmalloc.h>

static inline void init_tcmalloc_profile(void)
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
static inline void init_tcmalloc_profile(void)
{
}
#endif /* USE_TCMALLOC_PROFILE */

extern char **environ;

/*
 * same preparation as in PostgreSQL (src/backend/utils/misc/ps_status.c, PS_USE_CLOBBER_ARGV).
 */
static char **prepare_proctitle_buffer(od_instance_t *instance, int argc,
				       char **argv)
{
	char *end_of_area = NULL;
	int i;

	for (i = 0; i < argc; i++) {
		if (i == 0 || end_of_area + 1 == argv[i]) {
			end_of_area = argv[i] + strlen(argv[i]);
		}
	}

	if (end_of_area == NULL) {
		instance->orig_argv_ptr = NULL;
		instance->orig_argv_ptr_len = 0;
		return argv;
	}

	for (i = 0; environ[i] != NULL; i++) {
		if (end_of_area + 1 == environ[i]) {
			end_of_area = environ[i] + strlen(environ[i]);
		}
	}

	instance->orig_argv_ptr = argv[0];
	instance->orig_argv_ptr_len = end_of_area - argv[0];

	char **new_environ = (char **)malloc((i + 1) * sizeof(char *));
	if (new_environ == NULL) {
		instance->orig_argv_ptr = NULL;
		instance->orig_argv_ptr_len = 0;
		return argv;
	}
	for (i = 0; environ[i] != NULL; i++) {
		new_environ[i] = strdup(environ[i]);
		if (new_environ[i] == NULL) {
			new_environ[i] = NULL;
			while (--i >= 0) {
				free(new_environ[i]);
			}
			free(new_environ);
			instance->orig_argv_ptr = NULL;
			instance->orig_argv_ptr_len = 0;
			return argv;
		}
	}
	new_environ[i] = NULL;
	environ = new_environ;

	char **new_argv = (char **)malloc((argc + 1) * sizeof(char *));
	if (new_argv == NULL) {
		return argv;
	}
	for (i = 0; i < argc; i++) {
		new_argv[i] = strdup(argv[i]);
		if (new_argv[i] == NULL) {
			new_argv[i] = NULL;
			while (--i >= 0) {
				free(new_argv[i]);
			}
			free(new_argv);
			return argv;
		}
	}
	new_argv[argc] = NULL;

#ifdef __darwin__
	/*
	 * macOS keeps a static copy of the argv pointer; update it
	 */
	*_NSGetArgv() = new_argv;
#endif

	/*
	 * make the remaining original argv slots (beyond argv[0]) point at
	 * the end of the clobber area (a NUL), so that ps shows a single
	 * string instead of concatenating stale argv entries
	 */
	for (i = 1; i < argc; i++) {
		argv[i] = end_of_area;
	}

	return new_argv;
}

int main(int argc, char *argv[], char *envp[])
{
	init_tcmalloc_profile();

	od_instance_t *odyssey = od_instance_create();
	char **argv_for_parse = prepare_proctitle_buffer(odyssey, argc, argv);
	(void)envp;
	int rc = od_instance_main(odyssey, argc, argv_for_parse, environ);
	if (rc == -1) {
		rc = EXIT_FAILURE;
	} else {
		rc = EXIT_SUCCESS;
	}

	return rc;
}

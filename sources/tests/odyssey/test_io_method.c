#include <odyssey.h>
#include <tests/odyssey_test.h>

#include <cfg/convert.h>
#include <cfg/reader.h>
#include <global.h>
#include <machinarium/build.h>
#include <rules.h>

#if defined(__linux__)
#define NATIVE_METHOD "epoll"
#define OTHER_METHOD "kqueue"
#else
#define NATIVE_METHOD "kqueue"
#define OTHER_METHOD "epoll"
#endif

#ifdef HAVE_IO_URING
#define AVAILABLE_METHODS NATIVE_METHOD ", io_uring"
#else
#define AVAILABLE_METHODS NATIVE_METHOD
#endif

static void check_io_method(const char *method, const char *expected)
{
	char path[] = "/tmp/odyssey_io_method_XXXXXX";
	int fd = mkstemp(path);
	test(fd != -1);
	FILE *file = fdopen(fd, "w");
	test(file != NULL);
	fputs("listen { host \"localhost\" port 6432 }\n"
	      "storage \"console\" { type \"local\" }\n",
	      file);
	if (method != NULL) {
		fprintf(file, "io_method \"%s\"\n", method);
	}
	test(fclose(file) == 0);

	od_cfg_model_t model;
	od_cfg_diag_list_t diags;
	od_cfg_model_init(&model);
	od_cfg_diag_list_init(&diags);
	int rc = od_cfg_parse_file_with_autoconf(path, NULL, &model, &diags);
	test(unlink(path) == 0);

	od_config_t config;
	od_rules_t rules;
	od_global_t global = { 0 };
	od_hba_rules_t hba;
	od_config_init(&config);
	od_rules_init(&rules);
	od_hba_rules_init(&hba);
	if (rc == 0) {
		rc = od_cfg_convert_model(&model, &config, &rules, &global,
					  &hba, &diags);
	}
	if (expected != NULL) {
		test(rc == 0);
		const od_config_field_t *field =
			od_config_field_by_key("io_method");
		test(field != NULL && !field->reloadable);
		char value[32];
		od_config_field_value(&config, field, value, sizeof(value));
		test(strcmp(value, expected) == 0);

		od_config_t running;
		od_config_init(&running);
		od_config_reload(&running, &config);
		od_config_field_value(&running, field, value, sizeof(value));
		test(strcmp(value, NATIVE_METHOD) == 0);
		od_config_free(&running);
	} else {
		test(rc == -1);
		test(diags.count == 1 &&
		     diags.items[0].level == OD_CFG_DIAG_ERROR);
		char error[128];
		snprintf(error, sizeof(error),
			 "invalid io_method '%s' (available: %s)", method,
			 AVAILABLE_METHODS);
		test(strcmp(diags.items[0].message, error) == 0);
	}
	od_hba_rules_free(&hba);
	od_rules_cleanup(&rules);
	od_rules_free(&rules);
	od_config_free(&config);
	od_cfg_model_free(&model);
	od_cfg_diag_list_free(&diags);
}

void odyssey_test_io_method(void)
{
	check_io_method(NULL, NATIVE_METHOD);
	check_io_method(NATIVE_METHOD, NATIVE_METHOD);
	check_io_method("unknown", NULL);
	check_io_method(OTHER_METHOD, NULL);
#ifdef HAVE_IO_URING
	check_io_method("io_uring", "io_uring");
#else
	check_io_method("io_uring", NULL);
#endif
}

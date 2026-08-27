

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <util.h>
#include <option.h>

void od_print_version(void)
{
	char features[128];
	fill_supported_features_string(features, sizeof(features));

#ifdef ODYSSEY_VERSION_GIT
	printf("%s %s (git %s) %s%s\ncompiled by %s\n", ODYSSEY_NAME,
	       ODYSSEY_VERSION_NUMBER, ODYSSEY_VERSION_GIT, ODYSSEY_BUILD_TYPE,
	       features, ODYSSEY_COMPILER_STRING);
#else
	printf("%s %s %s%s\ncompiled by %s\n", ODYSSEY_NAME,
	       ODYSSEY_VERSION_NUMBER, ODYSSEY_BUILD_TYPE, features,
	       ODYSSEY_COMPILER_STRING);
#endif
}

int od_format_version_string(char *buf, size_t size)
{
#ifdef ODYSSEY_VERSION_GIT
	return od_snprintf(buf, size, "%s %s (git %s) %s, compiled by %s",
			   ODYSSEY_NAME, ODYSSEY_VERSION_NUMBER,
			   ODYSSEY_VERSION_GIT, ODYSSEY_BUILD_TYPE,
			   ODYSSEY_COMPILER_STRING);
#else
	return od_snprintf(buf, size, "%s %s %s, compiled by %s", ODYSSEY_NAME,
			   ODYSSEY_VERSION_NUMBER, ODYSSEY_BUILD_TYPE,
			   ODYSSEY_COMPILER_STRING);
#endif
}

void od_print_help(const char *program)
{
	printf("Usage: %s [OPTION...] /path/to/odyssey.conf\n", program);
	printf("Odyssey - scalable postgresql connection pooler\n\n");
	printf("      --console              Do not fork on startup\n");
	printf("      --log_to_stdout        Log to stdout\n");
	printf("      --silent               Do not log anything\n");
	printf("      --test                 Configuration testing\n");
	printf("      --verbose              Log everything\n");
	printf("  -V, --version              Print program version\n");
	printf("  -h, --help                 Give this help list\n");
	printf("  -?                        Same as --help\n");
	printf("Report bugs to <x4mmm@yandex-team.ru> or <rkhapov@yandex-team.ru>.\n");
}

od_retcode_t od_apply_validate_cli_args(od_logger_t *logger, od_config_t *conf,
					od_arguments_t *args, od_rules_t *rules)
{
	if (conf->daemonize && !args->console) {
		conf->daemonize |= args->console;
	} else {
		conf->daemonize = 0;
	}

	if (args->silent && args->verbose) {
		od_log(logger, "startup", NULL, NULL,
		       "silent and verbose option both specified");
		return NOT_OK_RESPONSE;
	}

	if (args->silent) {
		conf->log_debug = 0;
		conf->log_session = 0;
		conf->log_query = 0;
		conf->log_session = 0;
		conf->log_stats = 0;

		od_list_t *i;
		od_list_foreach (&rules->rules, i) {
			od_rule_t *rule;
			rule = od_container_of(i, od_rule_t, link);

			rule->log_query = 0;
			rule->log_debug = 0;
		}
	}

	if (args->verbose) {
		conf->log_debug = 1;
		conf->log_session = 1;
		conf->log_query = 1;
		conf->log_session = 1;
		conf->log_stats = 1;

		od_list_t *i;
		od_list_foreach (&rules->rules, i) {
			od_rule_t *rule;
			rule = od_container_of(i, od_rule_t, link);

			rule->log_query = 1;
			rule->log_debug = 1;
		}
	}

	if (args->log_stdout) {
		conf->log_to_stdout = 1;
	}

	return OK_RESPONSE;
}

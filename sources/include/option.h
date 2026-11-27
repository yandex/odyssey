#pragma once

#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include <types.h>
#include <instance.h>
#include <rules.h>

extern void od_usage(od_instance_t *instance, char *path);
extern int od_config_testing(od_instance_t *instance);
extern void od_print_version(void);
extern void od_print_help(const char *program);
extern void fill_supported_features_string(char *out, size_t max);
extern int od_format_version_string(char *buf, size_t size);

typedef struct {
	od_instance_t *instance;
	int silent;
	int verbose;
	int console;
	int log_stdout;
	int test;
} od_arguments_t;

typedef enum {
	OD_OPT_CONSOLE = 10001, /* >= than any utf symbol like -q -l etc */
	OD_OPT_SILENT,
	OD_OPT_VERBOSE,
	OD_OPT_LOG_STDOUT,
	OD_OPT_TEST,
	OD_OPT_VERSION,
	OD_OPT_USAGE,
	OD_OPT_CONFIG_DIR,
} od_cli_options;

static const char *short_opts = "Vh?";

static struct option long_opts[] = {
	{ "verbose", no_argument, NULL, OD_OPT_VERBOSE },
	{ "silent", no_argument, NULL, OD_OPT_SILENT },
	{ "console", no_argument, NULL, OD_OPT_CONSOLE },
	{ "log_to_stdout", no_argument, NULL, OD_OPT_LOG_STDOUT },
	{ "test", no_argument, NULL, OD_OPT_TEST },
	{ "config-dir", required_argument, NULL, OD_OPT_CONFIG_DIR },
	{ "version", no_argument, NULL, OD_OPT_VERSION },
	{ "help", no_argument, NULL, 'h' },
	{ "usage", no_argument, NULL, OD_OPT_USAGE },
	{ 0, 0, 0, 0 }
};

static inline void od_parse_args(int argc, char **argv, od_arguments_t *args)
{
	od_instance_t *instance = args->instance;
	const char *config_dir = NULL;

	for (;;) {
		int opt_idx = 0;
		int c = getopt_long(argc, argv, short_opts, long_opts,
				    &opt_idx);
		if (c == -1) {
			break;
		}

		switch (c) {
		case OD_OPT_SILENT:
			args->silent = 1;
			break;
		case OD_OPT_VERBOSE:
			args->verbose = 1;
			break;
		case 'h':
			od_print_help(argv[0]);
			exit(0);
		case OD_OPT_VERSION:
		case 'V':
			od_print_version();
			exit(0);
		case OD_OPT_USAGE:
			printf("Usage: %s [-?V] [--console] [--log_to_stdout] "
			       "[--silent] [--test] [--verbose] [--help] "
			       "[--usage] [--version] (CONFIG_FILE | --config-dir DIR)\n",
			       argv[0]);
			exit(0);
		case OD_OPT_CONSOLE:
			args->console = 1;
			break;
		case OD_OPT_LOG_STDOUT:
			args->log_stdout = 1;
			break;
		case OD_OPT_TEST:
			args->test = 1;
			break;
		case OD_OPT_CONFIG_DIR:
			if (config_dir != NULL || optarg[0] == '\0') {
				fprintf(stderr,
					"--config-dir requires a single non-empty directory path\n");
				exit(1);
			}
			config_dir = optarg;
			break;
		case '?':
			if (optopt == 0 && optind > 0 &&
			    argv[optind - 1] != NULL &&
			    strcmp(argv[optind - 1], "-?") == 0) {
				od_print_help(argv[0]);
				exit(0);
			}
			fprintf(stderr,
				"Try `%s --help' for more information.\n",
				argv[0]);
			exit(1);
		default:
			od_usage(instance, instance->exec_path);
			exit(1);
		}
	}

	if (argc - optind != (config_dir ? 0 : 1)) {
		od_usage(instance, instance->exec_path);
		exit(1);
	}

	instance->config_is_dir = config_dir != NULL;
	instance->config_path =
		od_strdup(config_dir ? config_dir : argv[optind]);
	if (instance->config_path == NULL) {
		fprintf(stderr, "out of memory copying configuration path\n");
		exit(1);
	}

	if (args->test == 1) {
		exit(od_config_testing(instance));
	}
}

extern od_retcode_t od_apply_validate_cli_args(od_logger_t *logger,
					       od_config_t *conf,
					       od_arguments_t *args,
					       od_rules_t *rules);

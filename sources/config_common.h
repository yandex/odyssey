#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

struct od_config_reader {
	od_parser_t parser;
	od_config_t *config;
	od_global_t *global;
	od_rules_t *rules;
	od_error_t *error;
	char *config_file;
	od_hba_rules_t *hba_rules;
	char *data;
	int data_size;
	regex_t rfc952_hostname_regex;
};

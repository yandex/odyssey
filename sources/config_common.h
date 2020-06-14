/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */
#ifndef ODYSSEY_CONFIG_COMMON_H
#define ODYSSEY_CONFIG_COMMON_H

#include "rules.h"
#include "error.h"
#include "parser.h"
#include "config.h"

typedef struct
{
	od_parser_t parser;
	od_config_t *config;
	od_rules_t *rules;
	od_error_t *error;
	char *config_file;
	char *data;
	int data_size;
} od_config_reader_t;

#endif // ODYSSEY_CONFIG_COMMON_H

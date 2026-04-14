#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/* Minimalistic for now */

#include <parser.h>

typedef enum {
	OD_QUERY_PROCESSING_LSET,
	OD_QUERY_PROCESSING_LTO,
	OD_QUERY_PROCESSING_LSHOW,
	OD_QUERY_PROCESSING_LODYSSEY,
	OD_QUERY_PROCESSING_LTSA,
	OD_QUERY_PROCESSING_LAPPNAME,
	OD_QUERY_PROCESSING_DEALLOCATE,
	OD_QUERY_PROCESSING_PREPARE,
	OD_QUERY_PROCESSING_ALL,
} od_query_processing_keywords_t;

extern od_keyword_t od_query_process_keywords[];

/*
 * Parse DEALLOCATE command: DEALLOCATE [ PREPARE ] { name | ALL }
 * Returns:
 *   1 if DEALLOCATE ALL detected
 *   0 if DEALLOCATE name detected (name and name_len are set)
 *  -1 on parse error
 */
int od_parse_deallocate(const char *query, size_t query_len, const char **name,
			size_t *name_len);
/* same as above, but for cases of parsing after reading DEALLOCATE on some parser */
int od_parse_deallocate_parser(od_parser_t *parser, const char **name,
			       size_t *name_len);

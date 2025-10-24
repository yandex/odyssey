#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/* Minimalistic for now */
typedef enum {
	OD_QUERY_PROCESSING_LSET,
	OD_QUERY_PROCESSING_LTO,
	OD_QUERY_PROCESSING_LSHOW,
	OD_QUERY_PROCESSING_LODYSSEY,
	OD_QUERY_PROCESSING_LTSA,
	OD_QUERY_PROCESSING_LAPPNAME,
} od_query_processing_keywords_t;

extern od_keyword_t od_query_process_keywords[];

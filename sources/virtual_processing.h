#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/* Minimalistic for now */
typedef enum {
	OD_VIRTUAL_LSET,
	OD_VIRTUAL_LSHOW,
	OD_VIRTUAL_LODYSSEY,
	OD_VIRTUAL_LTSA,
} od_virtual_keywords_t;

extern od_keyword_t od_virtual_process_keywords[];

#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>

int od_sql_minimal_keyword_lookup(const char *str, size_t len);

/*
 * whether a keyword token (as returned by od_sql_minimal_keyword_lookup)
 * can be the first token of a statement accepted by the grammar
 */
int od_sql_minimal_keyword_starts_stmt(int token);

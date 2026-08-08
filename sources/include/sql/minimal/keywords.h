#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>

int od_sql_minimal_keyword_lookup(const char *str, size_t len);

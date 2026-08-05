#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>

int od_cfg_keyword_lookup(const char *str, size_t len);

const char *od_cfg_keyword_suggest(const char *str, size_t len,
				   int max_distance);

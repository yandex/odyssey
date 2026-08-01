#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>

#include <od_memory.h>

typedef struct {
	const char *filename;

	int first_line;
	int first_column;
	int last_line;
	int last_column;

	size_t first_offset;
	size_t last_offset;
} od_cfg_location_t;

static inline od_cfg_location_t od_cfg_location_empty(const char *filename)
{
	od_cfg_location_t loc;

	loc.filename = filename;

	loc.first_line = 1;
	loc.first_column = 1;
	loc.last_line = 1;
	loc.last_column = 1;

	loc.first_offset = 0;
	loc.last_offset = 0;

	return loc;
}

static inline od_cfg_location_t od_cfg_location_copy(od_cfg_location_t loc)
{
	loc.filename = loc.filename ? od_strdup(loc.filename) : NULL;
	return loc;
}

static inline void od_cfg_location_free(od_cfg_location_t *loc)
{
	od_free((char *)loc->filename);
	loc->filename = NULL;
}

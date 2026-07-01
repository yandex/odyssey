#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>
#include <stdarg.h>

#include <cfg/loc.h>

typedef enum {
	OD_CFG_DIAG_ERROR,
	OD_CFG_DIAG_WARNING,
} od_cfg_diag_level_t;

typedef struct {
	od_cfg_diag_level_t level;
	od_cfg_location_t location;
	char *message;
	char *hint;
} od_cfg_diag_t;

typedef struct {
	od_cfg_diag_t *items;
	size_t count;
	size_t capacity;
} od_cfg_diag_list_t;

void od_cfg_diag_list_init(od_cfg_diag_list_t *diags);
void od_cfg_diag_list_free(od_cfg_diag_list_t *diags);

void od_cfg_diag_dumpf(FILE *file, const od_cfg_diag_list_t *diags);

void od_cfg_diag_error(od_cfg_diag_list_t *diags, od_cfg_location_t location,
		       const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

void od_cfg_diag_warning(od_cfg_diag_list_t *diags, od_cfg_location_t location,
			 const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

void od_cfg_diag_deprecated_ignored(od_cfg_diag_list_t *diags,
				    od_cfg_location_t location,
				    const char *option_name);

int od_cfg_diag_has_errors(const od_cfg_diag_list_t *diags);

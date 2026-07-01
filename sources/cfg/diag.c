/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <od_memory.h>
#include <cfg/diag.h>

static char *od_cfg_vformat(const char *fmt, va_list args)
{
	va_list copy;
	va_copy(copy, args);

	int len = vsnprintf(NULL, 0, fmt, copy);
	va_end(copy);

	if (len < 0) {
		return NULL;
	}

	char *buf = od_malloc((size_t)len + 1);
	if (buf == NULL) {
		return NULL;
	}

	vsnprintf(buf, (size_t)len + 1, fmt, args);
	return buf;
}

void od_cfg_diag_list_init(od_cfg_diag_list_t *diags)
{
	diags->items = NULL;
	diags->count = 0;
	diags->capacity = 0;
}

void od_cfg_diag_list_free(od_cfg_diag_list_t *diags)
{
	if (diags == NULL) {
		return;
	}

	for (size_t i = 0; i < diags->count; i++) {
		od_free(diags->items[i].message);
		od_free(diags->items[i].hint);
	}

	od_free(diags->items);

	diags->items = NULL;
	diags->count = 0;
	diags->capacity = 0;
}

static void od_cfg_diag_addv(od_cfg_diag_list_t *diags,
			     od_cfg_diag_level_t level,
			     od_cfg_location_t location, const char *fmt,
			     va_list args)
{
	if (diags->count == diags->capacity) {
		size_t new_capacity =
			diags->capacity == 0 ? 16 : diags->capacity * 2;
		od_cfg_diag_t *new_items = od_realloc(
			diags->items, sizeof(od_cfg_diag_t) * new_capacity);
		if (new_items == NULL) {
			return;
		}

		diags->items = new_items;
		diags->capacity = new_capacity;
	}

	od_cfg_diag_t *diag = &diags->items[diags->count++];

	diag->level = level;
	diag->location = location;
	diag->message = od_cfg_vformat(fmt, args);
	diag->hint = NULL;
}

void od_cfg_diag_error(od_cfg_diag_list_t *diags, od_cfg_location_t location,
		       const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_cfg_diag_addv(diags, OD_CFG_DIAG_ERROR, location, fmt, args);
	va_end(args);
}

void od_cfg_diag_warning(od_cfg_diag_list_t *diags, od_cfg_location_t location,
			 const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	od_cfg_diag_addv(diags, OD_CFG_DIAG_WARNING, location, fmt, args);
	va_end(args);
}

int od_cfg_diag_has_errors(const od_cfg_diag_list_t *diags)
{
	for (size_t i = 0; i < diags->count; i++) {
		if (diags->items[i].level == OD_CFG_DIAG_ERROR) {
			return 1;
		}
	}
	return 0;
}

void od_cfg_diag_deprecated_ignored(od_cfg_diag_list_t *diags,
				    od_cfg_location_t location,
				    const char *option_name)
{
	od_cfg_diag_warning(diags, location,
			    "option '%s' is deprecated and ignored",
			    option_name);
}

void od_cfg_diag_dumpf(FILE *file, const od_cfg_diag_list_t *diags)
{
	for (size_t i = 0; i < diags->count; i++) {
		const od_cfg_diag_t *diag = &diags->items[i];

		const char *level =
			diag->level == OD_CFG_DIAG_ERROR ? "error" : "warning";

		if (diag->location.filename != NULL) {
			fprintf(file, "%s:%d:%d: %s: %s\n",
				diag->location.filename,
				diag->location.first_line,
				diag->location.first_column, level,
				diag->message ? diag->message : "");
		} else {
			fprintf(file, "%s: %s\n", level,
				diag->message ? diag->message : "");
		}

		if (diag->hint != NULL) {
			fprintf(file, "  hint: %s\n", diag->hint);
		}
	}
}

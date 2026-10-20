#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Editing of the ALTER SYSTEM autoconf file text.
 */

/*
 * Applies one ALTER SYSTEM change to the text of the autoconf file.
 *
 *   key != NULL, line != NULL - SET: comment out active lines of key,
 *                               append line
 *   key != NULL, line == NULL - RESET: comment out active lines of key
 *   key == NULL               - RESET ALL: comment out every active line
 *
 * Old values are commented out, never removed, so the file keeps the
 * history of changes. An empty old_text gets the header first.
 *
 * Returns a newly allocated text (free with od_free) or NULL on out of
 * memory.
 */
char *od_autoconf_apply(const char *old_text, const char *key,
			const char *line);

/* refuse to grow the autoconf file past this size */
#define OD_AUTOCONF_MAX_SIZE (1024 * 1024)

/*
 * Checks a candidate autoconf file before it replaces the current one.
 * Returns 0 if the configuration with this file is acceptable.
 */
typedef int (*od_autoconf_check_cb)(const char *autoconf_path, void *arg);

/*
 * Applies one change to <config_path>.autoconf on disk: reads the file
 * (a missing file counts as empty), applies od_autoconf_apply(), checks
 * the result with the configuration parser and then with check, unless it
 * is NULL, and atomically replaces the file. On any error the old file
 * stays intact.
 *
 * Calls must not overlap. Odyssey runs them in the system thread, which
 * also has a stack large enough for the configuration parser.
 *
 * Returns 0 on success, -1 on error with a message in err.
 */
int od_autoconf_update(const char *config_path, const char *key,
		       const char *line, od_autoconf_check_cb check,
		       void *check_arg, char *err, size_t err_size);

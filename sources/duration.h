#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Duration values are parsed with the code ported from PostgreSQL
 * (src/backend/utils/misc/guc.c: parse_int() + convert_to_base_unit() and
 * the unit conversion tables), so configuration time values accept exactly
 * the PostgreSQL GUC syntax:
 *
 *     <number>[<unit>]
 *
 *   number  optional +/- sign, then decimal digits, 0x-prefixed
 *           hexadecimal digits, or a decimal float (1.5, .5, 1e3)
 *   unit    one of (case-sensitive, like PostgreSQL):
 *             us, ms, s, min, h, d
 *
 * Compound values ("1h30m") are not accepted — PostgreSQL does not
 * support them either.  Whitespace between the number and the unit is
 * allowed ("30 min"), trailing whitespace after the unit is allowed,
 * trailing garbage is rejected.  The numeric part may be a float, the
 * result is rounded half-to-even (rint) to whole milliseconds, exactly
 * like PostgreSQL rounds a GUC value to its base unit.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * A bare number or a PostgreSQL GUC-style duration ("500ms", "30 min",
 * "1.5h").  Unitless values retain the literal (possibly negative) number
 * in bare or real, selected by is_real.  The caller applies the option's
 * historic base unit before rounding to milliseconds.  Values with an
 * explicit unit have has_unit set and are already converted to ms.
 */
typedef struct {
	int has_unit;
	int is_real;
	int64_t bare;
	double real;
	int64_t ms;
} od_duration_value_t;

/* Explicit units use ms; otherwise bare or real retains the input value. */
bool od_duration_parse_value(const char *value, od_duration_value_t *out,
			     const char **hintmsg);

/*
 * Parse a PostgreSQL-style duration into whole milliseconds.
 *
 * The base unit is milliseconds (GUC_UNIT_MS), so a bare number means
 * milliseconds; values with an explicit unit are converted to
 * milliseconds.  Per-option historic units for bare numbers are applied
 * by the configuration grammar, not here.
 *
 * `value` must be NUL-terminated.
 *
 * Returns true on success.  On failure *hintmsg (if requested) points to
 * a static hint string in the PostgreSQL GUC style, or is NULL.
 */
bool od_duration_parse_ms(const char *value, int64_t *out_ms,
			  const char **hintmsg);

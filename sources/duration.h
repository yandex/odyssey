#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * od_duration_parse --
 *
 * Parse a Go-style duration string from a raw byte buffer.
 * No dependency on od_parser_t — this is a pure string function.
 *
 * Recognised suffixes (case-insensitive):
 *   d   days          (86400 s)
 *   h   hours         (3600 s)
 *   m   minutes       (60 s)
 *   s   seconds
 *   ms  milliseconds
 *   us  microseconds
 *   ns  nanoseconds   (truncated to microseconds)
 *
 * Compound expressions like "1h30m" or "1d2h3m4s" are supported.
 *
 * If a component carries no recognised suffix, the bare number is multiplied
 * by default_unit_us (in microseconds) for backward compatibility:
 *   default_unit_us = 1000      — bare number means milliseconds
 *   default_unit_us = 1000000  — bare number means seconds
 *
 * Leading whitespace is consumed and included in the returned count.
 *
 * Returns: number of bytes consumed (>= 1) on success,
 *          -1 if the input does not start with a digit (after whitespace).
 * On success *out_us holds the total duration in microseconds.
 */
int od_duration_parse(const char *str, size_t len, int64_t default_unit_us,
		      uint64_t *out_us);

/* Bare number = milliseconds; result in microseconds. */
static inline int od_duration_parse_ms(const char *str, size_t len,
				       uint64_t *out_us)
{
	return od_duration_parse(str, len, 1000LL, out_us);
}

/* Bare number = seconds; result in microseconds. */
static inline int od_duration_parse_s(const char *str, size_t len,
				      uint64_t *out_us)
{
	return od_duration_parse(str, len, 1000000LL, out_us);
}

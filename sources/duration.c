/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <duration.h>

/*
 * od_duration_parse --
 *
 * Pure string implementation — see duration.h for the full specification.
 *
 * The loop reads one <number><suffix> component per iteration.  When a
 * component has no recognised suffix the bare number is scaled by
 * default_unit_us and the loop stops (backward-compatible bare-integer
 * behaviour).
 */
int od_duration_parse(const char *str, size_t len, int64_t default_unit_us,
		      uint64_t *out_us)
{
	const char *p = str;
	const char *end = str + len;
	uint64_t total = 0;
	bool first = true;

	/* skip leading whitespace */
	while (p < end && isspace((unsigned char)*p)) {
		p++;
	}

	for (;;) {
		if (p >= end || !isdigit((unsigned char)*p)) {
			if (first) {
				return -1;
			}
			break;
		}

		/* read decimal integer */
		uint64_t value = 0;
		while (p < end && isdigit((unsigned char)*p)) {
			if (__builtin_mul_overflow(value, 10ULL, &value)) {
				return -1;
			}
			uint64_t digit = (uint64_t)((unsigned char)*p++ - '0');
			if (__builtin_add_overflow(value, digit, &value)) {
				return -1;
			}
		}
		first = false;

		/* read suffix (letters only) */
		const char *sfx = p;
		while (p < end && isalpha((unsigned char)*p)) {
			p++;
		}
		int sfx_len = (int)(p - sfx);

		uint64_t unit_us;
		if (sfx_len == 0) {
			if (default_unit_us < 0) {
				return -1;
			}
			unit_us = (uint64_t)default_unit_us;
		} else if (sfx_len == 1 &&
			   tolower((unsigned char)sfx[0]) == 'd') {
			unit_us = 86400000000ULL;
		} else if (sfx_len == 1 &&
			   tolower((unsigned char)sfx[0]) == 'h') {
			unit_us = 3600000000ULL;
		} else if (sfx_len == 1 &&
			   tolower((unsigned char)sfx[0]) == 'm') {
			unit_us = 60000000ULL;
		} else if (sfx_len == 1 &&
			   tolower((unsigned char)sfx[0]) == 's') {
			unit_us = 1000000ULL;
		} else if (sfx_len == 2 &&
			   tolower((unsigned char)sfx[0]) == 'm' &&
			   tolower((unsigned char)sfx[1]) == 's') {
			unit_us = 1000ULL;
		} else if (sfx_len == 2 &&
			   tolower((unsigned char)sfx[0]) == 'u' &&
			   tolower((unsigned char)sfx[1]) == 's') {
			unit_us = 1ULL;
		} else if (sfx_len == 2 &&
			   tolower((unsigned char)sfx[0]) == 'n' &&
			   tolower((unsigned char)sfx[1]) == 's') {
			/* nanoseconds are truncated to microseconds */
			value /= 1000ULL;
			unit_us = 1ULL;
		} else {
			/* unrecognised suffix — stop and treat as bare number */
			p = sfx;
			if (default_unit_us < 0) {
				return -1;
			}
			unit_us = (uint64_t)default_unit_us;
		}

		uint64_t component;
		if (__builtin_mul_overflow(value, unit_us, &component)) {
			return -1;
		}
		if (__builtin_add_overflow(total, component, &total)) {
			return -1;
		}
	}

	*out_us = total;
	return (int)(p - str);
}

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
			value = value * 10 +
				(uint64_t)((unsigned char)*p++ - '0');
		}
		first = false;

		/* read suffix (letters only) */
		const char *sfx = p;
		while (p < end && isalpha((unsigned char)*p)) {
			p++;
		}
		int sfx_len = (int)(p - sfx);

		if (sfx_len == 0) {
			total += value * (uint64_t)default_unit_us;
			break;
		}

		if (sfx_len == 1 && tolower((unsigned char)sfx[0]) == 'd') {
			total += value * 86400000000ULL;
		} else if (sfx_len == 1 &&
			   tolower((unsigned char)sfx[0]) == 'h') {
			total += value * 3600000000ULL;
		} else if (sfx_len == 1 &&
			   tolower((unsigned char)sfx[0]) == 'm') {
			total += value * 60000000ULL;
		} else if (sfx_len == 1 &&
			   tolower((unsigned char)sfx[0]) == 's') {
			total += value * 1000000ULL;
		} else if (sfx_len == 2 &&
			   tolower((unsigned char)sfx[0]) == 'm' &&
			   tolower((unsigned char)sfx[1]) == 's') {
			total += value * 1000ULL;
		} else if (sfx_len == 2 &&
			   tolower((unsigned char)sfx[0]) == 'u' &&
			   tolower((unsigned char)sfx[1]) == 's') {
			total += value;
		} else if (sfx_len == 2 &&
			   tolower((unsigned char)sfx[0]) == 'n' &&
			   tolower((unsigned char)sfx[1]) == 's') {
			total += value / 1000ULL;
		} else {
			/* unrecognised suffix — stop and treat as bare number */
			p = sfx;
			total += value * (uint64_t)default_unit_us;
			break;
		}
	}

	*out_us = total;
	return (int)(p - str);
}

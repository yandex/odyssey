/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Portions of this file are copied from PostgreSQL, source files
 * src/backend/utils/misc/guc.c (parse_int, convert_to_base_unit and the
 * unit conversion tables) and src/include/utils/guc.h (GUC_UNIT flags).
 *
 * Porting notes, i.e. deviations from the original code:
 * - results are int64_t instead of int: Odyssey stores option values in
 *   64-bit millisecond fields, so the "Value exceeds integer range." check
 *   uses INT64_MAX/INT64_MIN;
 * - the integer scaling step uses exact 64-bit integer arithmetic instead
 *   of going through double, to keep large values precise;
 * - strtol() with base 0 is replaced with explicit base-10 / 0x-hexadecimal
 *   parsing: leading-zero decimal values ("010") must keep their decimal
 *   meaning for backward compatibility with existing Odyssey config files;
 * - gettext_noop() is dropped (Odyssey has no message translation);
 * - the table end markers are spelled {"", 0, 0.0} to keep
 *   -Wmissing-field-initializers quiet.
 */

#include <odyssey.h>

#include <duration.h>

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------
 * GUC_UNIT flags, ported from src/include/utils/guc.h
 *-------------------------------------------------------------------------
 */
#define GUC_UNIT_KB 0x01000000 /* value is in kilobytes */
#define GUC_UNIT_BLOCKS 0x02000000 /* value is in blocks */
#define GUC_UNIT_XBLOCKS 0x03000000 /* value is in xlog blocks */
#define GUC_UNIT_MB 0x04000000 /* value is in megabytes */
#define GUC_UNIT_BYTE 0x05000000 /* value is in bytes */
#define GUC_UNIT_MEMORY 0x0F000000 /* mask for size-related units */

#define GUC_UNIT_MS 0x10000000 /* value is in milliseconds */
#define GUC_UNIT_S 0x20000000 /* value is in seconds */
#define GUC_UNIT_MIN 0x30000000 /* value is in minutes */
#define GUC_UNIT_TIME 0x70000000 /* mask for time-related units */

#define GUC_UNIT (GUC_UNIT_MEMORY | GUC_UNIT_TIME)

/*-------------------------------------------------------------------------
 * Unit conversion tables and hints, ported from guc.c
 *-------------------------------------------------------------------------
 */
#define MAX_UNIT_LEN 3 /* length of longest recognized unit string */

typedef struct {
	char unit[MAX_UNIT_LEN + 1]; /* unit, as a string, like "kB" or
				      * "min" */
	int base_unit; /* GUC_UNIT_XXX */
	double multiplier; /* Factor for converting unit -> base_unit */
} unit_conversion;

static const char *const memory_units_hint =
	"Valid units for this parameter are \"B\", \"kB\", \"MB\", \"GB\", "
	"and \"TB\".";

static const unit_conversion memory_unit_conversion_table[] = {
	{ "TB", GUC_UNIT_BYTE, 1024.0 * 1024.0 * 1024.0 * 1024.0 },
	{ "GB", GUC_UNIT_BYTE, 1024.0 * 1024.0 * 1024.0 },
	{ "MB", GUC_UNIT_BYTE, 1024.0 * 1024.0 },
	{ "kB", GUC_UNIT_BYTE, 1024.0 },
	{ "B", GUC_UNIT_BYTE, 1.0 },

	{ "TB", GUC_UNIT_KB, 1024.0 * 1024.0 * 1024.0 },
	{ "GB", GUC_UNIT_KB, 1024.0 * 1024.0 },
	{ "MB", GUC_UNIT_KB, 1024.0 },
	{ "kB", GUC_UNIT_KB, 1.0 },
	{ "B", GUC_UNIT_KB, 1.0 / 1024.0 },

	{ "TB", GUC_UNIT_MB, 1024.0 * 1024.0 },
	{ "GB", GUC_UNIT_MB, 1024.0 },
	{ "MB", GUC_UNIT_MB, 1.0 },
	{ "kB", GUC_UNIT_MB, 1.0 / 1024.0 },
	{ "B", GUC_UNIT_MB, 1.0 / (1024.0 * 1024.0) },

	{ "", 0, 0.0 } /* end of table marker */
};

static const char *const time_units_hint =
	"Valid units for this parameter are \"us\", \"ms\", \"s\", \"min\", "
	"\"h\", and \"d\".";

static const unit_conversion time_unit_conversion_table[] = {
	{ "d", GUC_UNIT_MS, 1000 * 60 * 60 * 24 },
	{ "h", GUC_UNIT_MS, 1000 * 60 * 60 },
	{ "min", GUC_UNIT_MS, 1000 * 60 },
	{ "s", GUC_UNIT_MS, 1000 },
	{ "ms", GUC_UNIT_MS, 1 },
	{ "us", GUC_UNIT_MS, 1.0 / 1000 },

	{ "d", GUC_UNIT_S, 60 * 60 * 24 },
	{ "h", GUC_UNIT_S, 60 * 60 },
	{ "min", GUC_UNIT_S, 60 },
	{ "s", GUC_UNIT_S, 1 },
	{ "ms", GUC_UNIT_S, 1.0 / 1000 },
	{ "us", GUC_UNIT_S, 1.0 / (1000 * 1000) },

	{ "d", GUC_UNIT_MIN, 60 * 24 },
	{ "h", GUC_UNIT_MIN, 60 },
	{ "min", GUC_UNIT_MIN, 1 },
	{ "s", GUC_UNIT_MIN, 1.0 / 60 },
	{ "ms", GUC_UNIT_MIN, 1.0 / (1000 * 60) },
	{ "us", GUC_UNIT_MIN, 1.0 / (1000 * 1000 * 60) },

	{ "", 0, 0.0 } /* end of table marker */
};

/*
 * convert_to_base_unit, ported from guc.c
 */
static bool convert_to_base_unit(double value, const char *unit, int base_unit,
				 double *base_value)
{
	char unitstr[MAX_UNIT_LEN + 1];
	int unitlen;
	const unit_conversion *table;

	/* extract unit string to compare to table entries */
	unitlen = 0;
	while (*unit != '\0' && !isspace((unsigned char)*unit) &&
	       unitlen < MAX_UNIT_LEN) {
		unitstr[unitlen++] = *(unit++);
	}
	unitstr[unitlen] = '\0';
	/* allow whitespace after unit */
	while (isspace((unsigned char)*unit)) {
		unit++;
	}
	if (*unit != '\0') {
		return false; /* unit too long, or garbage after it */
	}

	/* now search the appropriate table */
	if (base_unit & GUC_UNIT_MEMORY) {
		table = memory_unit_conversion_table;
	} else {
		table = time_unit_conversion_table;
	}

	for (int i = 0; *table[i].unit; i++) {
		if (base_unit == table[i].base_unit &&
		    strcmp(unitstr, table[i].unit) == 0) {
			double cvalue = value * table[i].multiplier;

			/*
			 * If the user gave a fractional value such as "30.1GB", round it
			 * off to the nearest multiple of the next smaller unit, if there
			 * is one.
			 */
			if (*table[i + 1].unit &&
			    base_unit == table[i + 1].base_unit) {
				cvalue =
					rint(cvalue / table[i + 1].multiplier) *
					table[i + 1].multiplier;
			}

			*base_value = cvalue;
			return true;
		}
	}
	return false;
}

static const char *time_units_hint_or(const int flags, const char **hintmsg)
{
	if (hintmsg != NULL) {
		*hintmsg = (flags & GUC_UNIT_MEMORY) ? memory_units_hint :
						       time_units_hint;
	}
	return NULL;
}

/*
 * parse_int, ported from guc.c with the adaptations listed in the file
 * header (64-bit result, exact integer scaling, no octal).
 */
static bool parse_int64(const char *value, int64_t *result, int flags,
			const char **hintmsg)
{
	/*
	 * We assume here that double is wide enough to represent any integer
	 * value with adequate precision.
	 */
	double val = 0.0;
	char *endptr;
	bool is_integer;
	bool is_exact;
	long long ll = 0;

	/* To suppress compiler warnings, always set output params */
	if (result) {
		*result = 0;
	}
	if (hintmsg) {
		*hintmsg = NULL;
	}

	/*
	 * Try to parse as an integer (decimal or 0x-hexadecimal input; no
	 * octal, so that leading-zero decimal values keep their decimal
	 * meaning).  If the conversion stops at a decimal point or an
	 * exponent, or overflows, re-parse as float.  This should work fine
	 * as long as we have no unit names starting with 'e'.
	 */
	{
		const char *digits = value;
		int base = 10;

		if (*digits == '+' || *digits == '-') {
			digits++;
		}
		if (digits[0] == '0' &&
		    (digits[1] == 'x' || digits[1] == 'X')) {
			base = 16;
		}

		errno = 0;
		ll = strtoll(value, &endptr, base);
	}

	is_integer = !(endptr == value || errno == ERANGE || *endptr == '.' ||
		       *endptr == 'e' || *endptr == 'E');

	if (!is_integer) {
		/* re-parse as float, like PostgreSQL's parse_int() */
		errno = 0;
		val = strtod(value, &endptr);
		if (endptr == value || errno == ERANGE) {
			return false; /* no HINT for these cases */
		}

		/* reject NaN (infinities will fail range check below) */
		if (isnan(val)) {
			return false; /* treat same as syntax error; no HINT */
		}

		is_exact = false;
	} else {
		val = (double)ll;
		is_exact = true;
	}

	/* allow whitespace between number and unit */
	while (isspace((unsigned char)*endptr)) {
		endptr++;
	}

	/* Handle possible unit */
	if (*endptr != '\0') {
		if ((flags & GUC_UNIT) == 0) {
			return false; /* this setting does not accept a unit */
		}

		double unit_mult;

		if (!convert_to_base_unit(1.0, endptr, flags, &unit_mult)) {
			/* invalid unit, or garbage after the unit; set hint and fail. */
			time_units_hint_or(flags, hintmsg);
			return false;
		}

		if (is_exact && unit_mult == floor(unit_mult)) {
			/*
			 * Scale with exact 64-bit integer arithmetic to avoid
			 * double precision loss for large values.
			 */
			long long scaled;

			if (__builtin_mul_overflow(ll, (long long)unit_mult,
						   &scaled)) {
				if (hintmsg) {
					*hintmsg =
						"Value exceeds integer range.";
				}
				return false;
			}

			if (result) {
				*result = scaled;
			}
			return true;
		}

		if (!convert_to_base_unit(val, endptr, flags, &val)) {
			/* invalid unit, or garbage after the unit; set hint and fail. */
			time_units_hint_or(flags, hintmsg);
			return false;
		}
	} else if (is_exact) {
		/* bare integer value, no scaling needed */
		if (result) {
			*result = ll;
		}
		return true;
	}

	/* Round to int, then check for overflow */
	val = rint(val);

	if (val >= (double)INT64_MAX || val < (double)INT64_MIN) {
		if (hintmsg) {
			*hintmsg = "Value exceeds integer range.";
		}
		return false;
	}

	if (result) {
		*result = (int64_t)val;
	}
	return true;
}

bool od_duration_parse_ms(const char *value, int64_t *out_ms,
			  const char **hintmsg)
{
	return parse_int64(value, out_ms, GUC_UNIT_MS, hintmsg);
}

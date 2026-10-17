/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <duration.h>
#include <tests/odyssey_test.h>

#include <string.h>

/*
 * The duration parser is ported from PostgreSQL guc.c (parse_int +
 * convert_to_base_unit), so these tests document PostgreSQL GUC semantics:
 * a single optional unit ("us", "ms", "s", "min", "h", "d"), case
 * sensitivity, floats, 0x-hexadecimal numbers, whitespace between the
 * number and the unit, rint() rounding to the base unit, and rejection of
 * compound values.
 */

static int64_t parse_ms(const char *input)
{
	int64_t ms = 0;
	const char *hint = NULL;

	test(od_duration_parse_ms(input, &ms, &hint));
	return ms;
}

static bool try_parse_ms(const char *input, int64_t *out_ms, const char **hint)
{
	return od_duration_parse_ms(input, out_ms, hint);
}

/* one number, one unit — every unit of the PostgreSQL time table */
static void test_units(void)
{
	test(parse_ms("500") == 500); /* bare = base unit (ms) */
	test(parse_ms("500ms") == 500);
	test(parse_ms("30s") == 30000);
	test(parse_ms("1min") == 60000);
	test(parse_ms("2h") == 7200000);
	test(parse_ms("1d") == 86400000);
}

static void test_units_whitespace(void)
{
	/* PostgreSQL allows whitespace between the number and the unit */
	test(parse_ms("30 min") == 1800000);
	test(parse_ms("5 s") == 5000);
	test(parse_ms("1h") == 3600000);
	test(parse_ms("30 min ") == 1800000); /* trailing whitespace */
}

/* units are compared with strcmp, exactly like PostgreSQL */
static void test_case_sensitive(void)
{
	int64_t ms;
	const char *hint;

	test(!try_parse_ms("1S", &ms, &hint));
	test(!try_parse_ms("500MS", &ms, &hint));
	test(!try_parse_ms("90MIN", &ms, &hint));
	test(!try_parse_ms("1H", &ms, &hint));
	test(!try_parse_ms("5m", &ms, &hint)); /* "m" is not a PG time unit */
	test(!try_parse_ms("5D", &ms, &hint));
}

/* compound values are not supported by PostgreSQL */
static void test_compound_rejected(void)
{
	int64_t ms;
	const char *hint = NULL;

	test(!try_parse_ms("1h30m", &ms, &hint));
	test(hint != NULL); /* invalid unit hint is set */
	test(!try_parse_ms("1m30s", &ms, &hint));
	test(!try_parse_ms("1d2h3m4s", &ms, &hint));
	test(!try_parse_ms("5mins", &ms, &hint)); /* garbage after the unit */
	test(!try_parse_ms("5s3", &ms, &hint));
	test(!try_parse_ms("5s x", &ms, &hint));
}

/* PostgreSQL's parse_int() re-parses as float for '.'/'e' */
static void test_floats(void)
{
	test(parse_ms("1.5s") == 1500);
	test(parse_ms("0.5s") == 500);
	test(parse_ms(".5s") == 500);
	test(parse_ms("1.h") == 3600000);
	test(parse_ms("1.5") == 2); /* bare float, rint(1.5 ms) */
	test(parse_ms("2.5") == 2); /* half-to-even */
	test(parse_ms("1.5 h") == 5400000);
}

/* PostgreSQL's parse_int() uses strtol(..., 0), we support 0x (no octal) */
static void test_hex(void)
{
	test(parse_ms("0x10s") == 16000);
	test(parse_ms("0x10") == 16);
	test(parse_ms("-0x2min") == -120000);
}

static void test_signs(void)
{
	test(parse_ms("+5s") == 5000);
	test(parse_ms("-5s") == -5000);
	test(parse_ms("-1") == -1);
}

/*
 * Rounding to the base unit: rint(), half-to-even, exactly like
 * PostgreSQL rounds a GUC value.
 */
static void test_rint_rounding(void)
{
	test(parse_ms("1500us") == 2);
	test(parse_ms("500us") == 0);
	test(parse_ms("500.5us") == 1);
	test(parse_ms("0.5ms") == 0);
	test(parse_ms("1.5ms") == 2);
}

static void test_leading_whitespace(void)
{
	test(parse_ms("  30s") == 30000);
	test(parse_ms("\t1min") == 60000);
}

/* strtod fallback for values like "5e3", same as PostgreSQL */
static void test_exponent(void)
{
	test(parse_ms("5e3") == 5000); /* 5e3 base units (ms) */
	test(parse_ms("1e3s") == 1000000);
}

static void test_invalid_rejected(void)
{
	int64_t ms;
	const char *hint;

	/* no digits at all */
	test(!try_parse_ms("", &ms, &hint));
	test(!try_parse_ms("   ", &ms, &hint));
	test(!try_parse_ms("ms", &ms, &hint));
	test(!try_parse_ms("s", &ms, &hint));
	test(!try_parse_ms("notanumber", &ms, &hint));

	/* unknown unit */
	test(!try_parse_ms("5x", &ms, &hint));
	test(hint != NULL);

	/* overflow */
	test(!try_parse_ms("99999999999999999999d", &ms, &hint));
	test(!try_parse_ms("1e30s", &ms, &hint));
	test(!try_parse_ms("9223372036854775808ms", &ms, &hint));
}

static void test_boundary(void)
{
	test(parse_ms("9223372036854775807ms") ==
	     INT64_MAX); /* exact int64 max */
	test(parse_ms("-9223372036854775808ms") == INT64_MIN);
}

void odyssey_test_duration_parse(void)
{
	test_units();
	test_units_whitespace();
	test_case_sensitive();
	test_compound_rejected();
	test_floats();
	test_hex();
	test_signs();
	test_rint_rounding();
	test_leading_whitespace();
	test_exponent();
	test_invalid_rejected();
	test_boundary();
}

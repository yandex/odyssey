/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <duration.h>
#include <tests/odyssey_test.h>

/*
 * Helpers — call the pure string API directly, no od_parser_t needed.
 * parse_ms / parse_s assert that parsing succeeds and return microseconds.
 * try_parse_ms returns false when parsing fails (used for error-path tests).
 */
static uint64_t parse_ms(const char *input)
{
	uint64_t us = 0;
	test(od_duration_parse_ms(input, strlen(input), &us) > 0);
	return us;
}

static uint64_t parse_s(const char *input)
{
	uint64_t us = 0;
	test(od_duration_parse_s(input, strlen(input), &us) > 0);
	return us;
}

static bool try_parse_ms(const char *input, uint64_t *out_us)
{
	return od_duration_parse_ms(input, strlen(input), out_us) > 0;
}

static void test_suffix_ns(void)
{
	test(parse_ms("2000000ns") == 2000ULL);
}

static void test_suffix_us(void)
{
	test(parse_ms("5000us") == 5000ULL);
}

static void test_suffix_ms(void)
{
	test(parse_ms("500ms") == 500000ULL);
}

static void test_suffix_s(void)
{
	test(parse_ms("30s") == 30000000ULL);
}

static void test_suffix_m(void)
{
	test(parse_ms("1m") == 60000000ULL);
}

static void test_suffix_h(void)
{
	test(parse_ms("2h") == 7200000000ULL);
}

static void test_suffix_d(void)
{
	test(parse_ms("1d") == 86400000000ULL);
}

static void test_compound_1h30m(void)
{
	test(parse_ms("1h30m") == 5400000000ULL);
}

static void test_compound_1m30s(void)
{
	test(parse_ms("1m30s") == 90000000ULL);
}

static void test_compound_2d3h(void)
{
	test(parse_ms("2d3h") == 183600000000ULL);
}

static void test_compound_1d2h3m4s(void)
{
	uint64_t expected = 1ULL * 24 * 60 * 60 * 1000000 +
			    2ULL * 60 * 60 * 1000000 + 3ULL * 60 * 1000000 +
			    4ULL * 1000000;
	test(parse_ms("1d2h3m4s") == expected);
}

static void test_compound_1m500ms(void)
{
	test(parse_ms("1m500ms") == 60500000ULL);
}

/*
 * Backward compatibility: bare number with no suffix.
 *
 * These tests document that existing config files continue to work exactly
 * as they did before duration parsing was added.  Each test names the
 * affected field and shows the stored-value math so a reviewer can verify
 * the conversion without reading config_reader.c.
 */
static void test_compat_ms_field_bare_number(void)
{
	/*
	 * Fields like pool_timeout store milliseconds as int.
	 * Old config: pool_timeout 5000
	 * parse_ms("5000") -> 5 000 000 us; stored: (int)(us / 1000) = 5000 ms
	 */
	uint64_t us = parse_ms("5000");
	test(us == 5000000ULL);
	test((int)(us / 1000ULL) == 5000);
}

static void test_compat_s_field_bare_number_u64(void)
{
	/*
	 * Fields like pool_client_idle_timeout store microseconds as uint64_t.
	 * Old config: pool_client_idle_timeout 30
	 * parse_s("30") -> 30 000 000 us; stored directly as uint64_t
	 */
	uint64_t us = parse_s("30");
	test(us == 30000000ULL);
}

static void test_compat_s_field_bare_number_int(void)
{
	/*
	 * Fields like catchup_timeout store seconds as int.
	 * Old config: catchup_timeout 10
	 * parse_s("10") -> 10 000 000 us; stored: (int)(us / 1 000 000) = 10 s
	 */
	uint64_t us = parse_s("10");
	test(us == 10000000ULL);
	test((int)(us / 1000000ULL) == 10);
}

static void test_no_suffix_zero(void)
{
	test(parse_ms("0") == 0ULL);
	test(parse_s("0") == 0ULL);
}

/* Explicit suffix must win over the field's default unit. */
static void test_suffix_overrides_default(void)
{
	test(parse_s("30s") == 30000000ULL);
	test(parse_s("2m") == 120000000ULL);
	test(parse_s("1h30m") == 5400000000ULL);
}

static void test_case_insensitive(void)
{
	test(parse_ms("1S") == 1000000ULL);
	test(parse_ms("1M") == 60000000ULL);
	test(parse_ms("1H") == 3600000000ULL);
	test(parse_ms("1D") == 86400000000ULL);
	test(parse_ms("500MS") == 500000ULL);
}

static void test_zero_with_suffix(void)
{
	test(parse_ms("0s") == 0ULL);
	test(parse_ms("0m") == 0ULL);
	test(parse_ms("0d") == 0ULL);
}

static void test_one_week(void)
{
	test(parse_ms("7d") == 604800000000ULL);
}

/*
 * Nanoseconds are truncated to microseconds via integer division.
 * Values below 1000 ns round to 0 us.
 */
static void test_ns_truncation(void)
{
	test(parse_ms("1ns") == 0ULL);
	test(parse_ms("999ns") == 0ULL);
	test(parse_ms("1000ns") == 1ULL);
}

/*
 * od_duration_parse skips leading whitespace and includes it in the
 * returned consumed count.
 */
static void test_leading_whitespace(void)
{
	test(parse_ms("  30s") == 30000000ULL);
	test(parse_ms(" 1h30m") == 5400000000ULL);
}

/*
 * The parser must reject any input that does not start with a digit.
 */
static void test_invalid_returns_false(void)
{
	uint64_t us;

	/* alphabetic string — not a number at all */
	test(!try_parse_ms("notanumber", &us));

	/* suffix with no leading number */
	test(!try_parse_ms("ms", &us));
	test(!try_parse_ms("s", &us));
	test(!try_parse_ms("h", &us));

	/* empty string */
	test(!try_parse_ms("", &us));

	/* whitespace only */
	test(!try_parse_ms("   ", &us));

	/* sign not followed by digits */
	test(!try_parse_ms("+5s", &us));
}

void odyssey_test_duration_parse(void)
{
	test_suffix_ns();
	test_suffix_us();
	test_suffix_ms();
	test_suffix_s();
	test_suffix_m();
	test_suffix_h();
	test_suffix_d();

	test_compound_1h30m();
	test_compound_1m30s();
	test_compound_2d3h();
	test_compound_1d2h3m4s();
	test_compound_1m500ms();

	test_compat_ms_field_bare_number();
	test_compat_s_field_bare_number_u64();
	test_compat_s_field_bare_number_int();
	test_no_suffix_zero();

	test_suffix_overrides_default();
	test_case_insensitive();

	test_zero_with_suffix();
	test_one_week();
	test_ns_truncation();
	test_leading_whitespace();
	test_invalid_returns_false();
}

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <duration.h>
#include <cfg/reader.h>
#include <tests/odyssey_test.h>

#include <string.h>
#include <unistd.h>

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
	test(!try_parse_ms("-9223372036854775809", &ms, &hint));
	test(!try_parse_ms(" -9223372036854776000 ", &ms, &hint));
}

static void test_boundary(void)
{
	test(parse_ms("9223372036854775807ms") ==
	     INT64_MAX); /* exact int64 max */
	test(parse_ms("-9223372036854775808ms") == INT64_MIN);
	od_duration_value_t parsed;
	test(od_duration_parse_value("-9223372036854775808", &parsed, NULL));
	test(!parsed.has_unit && !parsed.is_real);
	test(parsed.bare == INT64_MIN);
	test(!od_duration_parse_value("-9223372036854775809", &parsed, NULL));
	test(!od_duration_parse_value("9223372036854775808", &parsed, NULL));
}

static void test_config_duration_strings(void)
{
	const struct {
		const char *value;
		int64_t seconds_ms;
		int64_t milliseconds;
	} cases[] = {
		{ "10", 10000, 10 },
		{ "\"10\"", 10000, 10 },
		{ "10s", 10000, 10000 },
		{ "\"10s\"", 10000, 10000 },
		{ "\"1.5s\"", 1500, 1500 },
		{ "1.5", 1500, 2 },
		{ "\"1.5\"", 1500, 2 },
		{ "\" 0x10 \"", 16000, 16 },
		{ "\"010\"", 10000, 10 },
		{ "\"1e3\"", 1000000, 1000 },
		{ "\"30 min\"", 1800000, 1800000 },
		{ "\"1000us\"", 1, 1 },
		{ "\"oops\"", -1, -1 },
		{ "\"10S\"", -1, -1 },
		{ "\"1h30m\"", -1, -1 },
		{ "\"-1s\"", -1, -1 },
		{ "\"9223372036854775808ms\"", -1, -1 },
		{ "\"2147483648ms\"", -1, -1 },
		{ "\"nan\"", -1, -1 },
		{ "\"inf\"", -1, -1 },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char path[] = "/tmp/odyssey_duration_XXXXXX";
		int fd = mkstemp(path);
		test(fd != -1);
		FILE *file = fdopen(fd, "w");
		test(file != NULL);
		fprintf(file,
			"stats_interval %s\nbackend_connect_timeout_ms %s\n"
			"cancel_queue_timeout_ms \"-1\"\n",
			cases[i].value, cases[i].value);
		test(fclose(file) == 0);
		od_cfg_model_t model;
		od_cfg_diag_list_t diags;
		od_cfg_model_init(&model);
		od_cfg_diag_list_init(&diags);
		int rc = od_cfg_parse_autoconf(path, &model, &diags);
		test(unlink(path) == 0);
		if (cases[i].seconds_ms >= 0) {
			if (rc != 0) {
				od_cfg_diag_dumpf(stderr, &diags);
			}
			test(rc == 0);
			test(!od_cfg_diag_has_errors(&diags));
			test(model.global.stats_interval_ms.value ==
			     (uint64_t)cases[i].seconds_ms);
			test(model.global.backend_connect_timeout_ms.value ==
			     (uint64_t)cases[i].milliseconds);
			test(model.global.cancel_queue_timeout_ms.value == -1);
		} else {
			test(rc != 0 || od_cfg_diag_has_errors(&diags));
		}
		od_cfg_diag_list_free(&diags);
		od_cfg_model_free(&model);
	}
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
	test_config_duration_strings();
}

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <misc.h>

#include <tests/odyssey_test.h>

/*
 * Canonical full words, case-insensitive.
 */
static void test_parse_bool_true_words(void)
{
	bool v;

	test(parse_bool("true", &v) && v);
	test(parse_bool("TRUE", &v) && v);
	test(parse_bool("True", &v) && v);
	test(parse_bool("tRue", &v) && v);

	test(parse_bool("yes", &v) && v);
	test(parse_bool("YES", &v) && v);
	test(parse_bool("Yes", &v) && v);

	test(parse_bool("on", &v) && v);
	test(parse_bool("ON", &v) && v);
	test(parse_bool("On", &v) && v);

	test(parse_bool("1", &v) && v);
}

static void test_parse_bool_false_words(void)
{
	bool v;

	test(parse_bool("false", &v) && !v);
	test(parse_bool("FALSE", &v) && !v);
	test(parse_bool("False", &v) && !v);
	test(parse_bool("fAlSe", &v) && !v);

	test(parse_bool("no", &v) && !v);
	test(parse_bool("NO", &v) && !v);
	test(parse_bool("No", &v) && !v);

	test(parse_bool("off", &v) && !v);
	test(parse_bool("OFF", &v) && !v);
	test(parse_bool("Off", &v) && !v);

	test(parse_bool("0", &v) && !v);
}

/*
 * parse_bool_with_len() bounds the comparison by len, so any prefix
 * of a valid word is accepted (same semantics as PostgreSQL).
 */
static void test_parse_bool_with_len_prefixes(void)
{
	bool v;

	test(parse_bool_with_len("trueX", 4, &v) && v);
	test(parse_bool_with_len("yes_no", 3, &v) && v);
	test(parse_bool_with_len("onX", 2, &v) && v);

	test(parse_bool_with_len("falseX", 5, &v) && !v);
	test(parse_bool_with_len("ofX", 2, &v) && !v);

	/* single-letter prefixes */
	test(parse_bool_with_len("t", 1, &v) && v);
	test(parse_bool_with_len("y", 1, &v) && v);
	test(parse_bool_with_len("f", 1, &v) && !v);
	test(parse_bool_with_len("n", 1, &v) && !v);

	/* 'o' alone is ambiguous and must be rejected (see misc.c) */
	test(!parse_bool_with_len("o", 1, &v));

	/* len must cover the digit */
	test(!parse_bool_with_len("1", 0, &v));
}

static void test_parse_bool_invalid(void)
{
	bool v = true;

	test(!parse_bool("", &v));
	test(!parse_bool("maybe", &v));
	test(!parse_bool("enabled", &v));
	test(!parse_bool("2", &v));
	test(!parse_bool("-1", &v));
	test(!parse_bool("10", &v));
	test(!parse_bool("01", &v));
	test(!parse_bool("truee", &v));
	test(!parse_bool("yes!", &v));
	test(!parse_bool("1kb", &v));

	/* no trimming: leading space is the caller's responsibility */
	test(!parse_bool(" true", &v));

	/* on failure *result is set to false */
	v = true;
	test(!parse_bool("nope", &v));
	test(!v);
}

static void test_parse_bool_null_result(void)
{
	test(parse_bool("true", NULL));
	test(!parse_bool("garbage", NULL));
}

void odyssey_test_misc(void)
{
	test_parse_bool_true_words();
	test_parse_bool_false_words();
	test_parse_bool_with_len_prefixes();
	test_parse_bool_invalid();
	test_parse_bool_null_result();
}

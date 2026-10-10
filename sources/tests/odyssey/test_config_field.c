#include <odyssey.h>
#include <tests/odyssey_test.h>

#include <config.h>

#define FORMAT_BUF_SIZE 256

static const char *format_ok(const char *key, const char *value, size_t size)
{
	static char buf[FORMAT_BUF_SIZE];

	const od_config_field_t *field = od_config_field_by_key(key);
	test(field != NULL);
	test(size <= sizeof(buf));

	memset(buf, 0, sizeof(buf));
	int rc = od_config_field_format(field, value, buf, size);
	if (rc != 0) {
		fprintf(stderr,
			"format_ok failed: key=[%s] value=[%s] size=%zu\n", key,
			value, size);
		abort();
	}
	return buf;
}

static void format_fail(const char *key, const char *value, size_t size)
{
	char buf[FORMAT_BUF_SIZE];

	const od_config_field_t *field = od_config_field_by_key(key);
	test(field != NULL);
	test(size <= sizeof(buf));

	int rc = od_config_field_format(field, value, buf, size);
	if (rc != -1) {
		fprintf(stderr,
			"format_fail: expected failure for key=[%s] value=[%s] "
			"size=%zu, got [%s]\n",
			key, value, size, buf);
		abort();
	}
}

static void test_field_by_key(void)
{
	const od_config_field_t *field = od_config_field_by_key("workers");
	test(field != NULL);
	test(strcmp(field->key, "workers") == 0);
	test(field->type == OD_CONFIG_FIELD_INT);

	test(od_config_field_by_key("nonexistent") == NULL);
	test(od_config_field_by_key("") == NULL);
	test(od_config_field_by_key("WORKERS") == NULL);
}

static void test_field_deprecated(void)
{
	test(od_config_field_deprecated(
		     od_config_field_by_key("graceful_die_on_errors")) == 1);
	test(od_config_field_deprecated(od_config_field_by_key("workers")) ==
	     0);
}

static void test_format_int(void)
{
	test(strcmp(format_ok("workers", "4", FORMAT_BUF_SIZE), "workers 4") ==
	     0);
	test(strcmp(format_ok("workers", "-1", FORMAT_BUF_SIZE),
		    "workers -1") == 0);
	test(strcmp(format_ok("workers", "007", FORMAT_BUF_SIZE),
		    "workers 007") == 0);

	format_fail("workers", "", FORMAT_BUF_SIZE);
	format_fail("workers", "-", FORMAT_BUF_SIZE);
	format_fail("workers", "abc", FORMAT_BUF_SIZE);
	format_fail("workers", "4a", FORMAT_BUF_SIZE);
	format_fail("workers", "0x10", FORMAT_BUF_SIZE);
	format_fail("workers", "1.5", FORMAT_BUF_SIZE);
	format_fail("workers", "+4", FORMAT_BUF_SIZE);
	format_fail("workers", " 4", FORMAT_BUF_SIZE);
}

static void test_format_workers_auto(void)
{
	test(strcmp(format_ok("workers", "auto", FORMAT_BUF_SIZE),
		    "workers \"auto\"") == 0);
	/* "auto" is special for workers only */
	format_fail("resolvers", "auto", FORMAT_BUF_SIZE);
	format_fail("workers", "AUTO", FORMAT_BUF_SIZE);
	/* "workers \"auto\"" is 14 chars + terminator */
	test(strcmp(format_ok("workers", "auto", 15), "workers \"auto\"") == 0);
	format_fail("workers", "auto", 14);
}

static void test_format_bool(void)
{
	test(strcmp(format_ok("log_debug", "yes", FORMAT_BUF_SIZE),
		    "log_debug yes") == 0);
	test(strcmp(format_ok("log_debug", "no", FORMAT_BUF_SIZE),
		    "log_debug no") == 0);
	test(strcmp(format_ok("log_debug", "YES", FORMAT_BUF_SIZE),
		    "log_debug yes") == 0);
	test(strcmp(format_ok("log_debug", "No", FORMAT_BUF_SIZE),
		    "log_debug no") == 0);

	format_fail("log_debug", "", FORMAT_BUF_SIZE);
	format_fail("log_debug", "on", FORMAT_BUF_SIZE);
	format_fail("log_debug", "true", FORMAT_BUF_SIZE);
	format_fail("log_debug", "1", FORMAT_BUF_SIZE);
}

static void test_format_string(void)
{
	/* OD_CONFIG_FIELD_STRING */
	test(strcmp(format_ok("log_format", "%p %m", FORMAT_BUF_SIZE),
		    "log_format \"%p %m\"") == 0);
	/* OD_CONFIG_FIELD_STRING_INLINE */
	test(strcmp(format_ok("availability_zone", "b", FORMAT_BUF_SIZE),
		    "availability_zone \"b\"") == 0);
	test(strcmp(format_ok("availability_zone", "", FORMAT_BUF_SIZE),
		    "availability_zone \"\"") == 0);
	test(strcmp(format_ok("availability_zone", "a\"b", FORMAT_BUF_SIZE),
		    "availability_zone \"a\\\"b\"") == 0);
	test(strcmp(format_ok("availability_zone", "a\\b", FORMAT_BUF_SIZE),
		    "availability_zone \"a\\b\"") == 0);

	format_fail("availability_zone", "a\nb", FORMAT_BUF_SIZE);
	format_fail("availability_zone", "a\rb", FORMAT_BUF_SIZE);
	/* would escape the closing quote */
	format_fail("availability_zone", "a\\", FORMAT_BUF_SIZE);
}

static void test_format_buffer_bounds(void)
{
	/* "workers 4" is 9 chars + terminator */
	test(strcmp(format_ok("workers", "4", 10), "workers 4") == 0);
	format_fail("workers", "4", 9);

	/* "log_debug yes" is 13 chars + terminator */
	test(strcmp(format_ok("log_debug", "yes", 14), "log_debug yes") == 0);
	format_fail("log_debug", "yes", 13);

	/* "availability_zone \"b\"" is 21 chars + terminator */
	test(strcmp(format_ok("availability_zone", "b", 22),
		    "availability_zone \"b\"") == 0);
	format_fail("availability_zone", "b", 21);

	/* "availability_zone \"\\\"\"" is 22 chars + terminator */
	test(strcmp(format_ok("availability_zone", "\"", 23),
		    "availability_zone \"\\\"\"") == 0);
	format_fail("availability_zone", "\"", 22);

	/* not even the key fits */
	format_fail("availability_zone", "b", 5);
}

void odyssey_test_config_field(void)
{
	test_field_by_key();
	test_field_deprecated();
	test_format_int();
	test_format_workers_auto();
	test_format_bool();
	test_format_string();
	test_format_buffer_bounds();
}

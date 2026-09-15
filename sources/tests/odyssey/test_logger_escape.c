/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>
#include <client.h>
#include <logger.h>

#include <tests/odyssey_test.h>

static void test_logger_output(od_client_t *client, char *format, char *message,
			       const char *expected, size_t expected_len)
{
	for (int async = 0; async <= 1; ++async) {
		od_logger_t logger;
		test(od_logger_init(&logger, NULL) == OK_RESPONSE);
		od_logger_set_stdout(&logger, 0);
		od_logger_set_async(&logger, async);
		od_logger_set_format(&logger, format);

		FILE *file = tmpfile();
		test(file != NULL);
		int fd = dup(fileno(file));
		test(fd != -1);
		atomic_store(&logger.fd, fd);

		od_logger_slot_t slot;
		memset(&slot, 0, sizeof(slot));
		if (async) {
			mm_lf_stack_push(&logger.free_slots, &slot.link);
			atomic_store(&logger.state, OD_LOGGER_ONLINE);
		}

		od_log(&logger, "test", client, NULL, "%s", message);
		od_logger_flush(&logger);
		if (async) {
			test(slot.len < sizeof(slot.text));
			test(slot.text[slot.len] == '\0');
		}

		char output[OD_LOGLINE_MAXLEN + 1];
		ssize_t len = pread(fd, output, sizeof(output), 0);
		test(len == (ssize_t)expected_len);
		test(memcmp(output, expected, expected_len) == 0);
		od_logger_close(&logger);
		test(fclose(file) == 0);
	}
}

void odyssey_test_logger_escape(void)
{
	od_client_t client;
	memset(&client, 0, sizeof(client));
	kiwi_be_startup_init(&client.startup);
	kiwi_vars_init(&client.vars);

	char *format = "tskv\\tuser=%u\\tdb=%d\\tapp=%a\\tid=%x\\tmsg=%M\\n";
	char *missing = "tskv\tuser=none\tdb=none\tapp=none\tid=none\tmsg=ok\n";
	test_logger_output(NULL, format, "ok", missing, strlen(missing));
	test_logger_output(&client, format, "ok", missing, strlen(missing));

	char too_long[KIWI_MAX_VAR_SIZE + 1];
	memset(too_long, 'a', sizeof(too_long) - 1);
	too_long[sizeof(too_long) - 1] = '\0';
	test(kiwi_vars_set(&client.vars, KIWI_VAR_APPLICATION_NAME, too_long,
			   sizeof(too_long)) == -1);
	test_logger_output(&client, format, "ok", missing, strlen(missing));

	struct {
		char *input;
		char *escaped;
	} cases[] = {
		{ "\nuser1", "\\nuser1" },
		{ "\r\tkey=value\\", "\\r\\tkey\\=value\\\\" },
		{ "\\n%u", "\\\\n%u" },
		{ "user 'name' \"app\"", "user 'name' \"app\"" },
		{ "пользователь", "пользователь" },
		{ "", "" },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		char *input = cases[i].input;
		char *escaped = cases[i].escaped;
		int len = strlen(input) + 1;
		test(kiwi_var_set(&client.startup.user, KIWI_VAR_UNDEF, input,
				  len) == 0);
		test(kiwi_var_set(&client.startup.database, KIWI_VAR_UNDEF,
				  input, len) == 0);
		test(kiwi_vars_set(&client.vars, KIWI_VAR_APPLICATION_NAME,
				   input, len) == 0);
		client.external_id = input;

		char expected[OD_LOGLINE_MAXLEN];
		int expected_len = snprintf(
			expected, sizeof(expected),
			"tskv\tuser=%s\tdb=%s\tapp=%s\tid=%s\tmsg=%s\n",
			escaped, escaped, escaped, escaped, escaped);
		test_logger_output(&client, format, input, expected,
				   expected_len);
		test(strcmp(client.startup.user.value, input) == 0);
		test(strcmp(client.startup.database.value, input) == 0);
		test(strcmp(kiwi_vars_get(&client.vars,
					  KIWI_VAR_APPLICATION_NAME)
				    ->value,
			    input) == 0);
	}
}

void odyssey_test_logger_escape_truncation(void)
{
	od_client_t client;
	memset(&client, 0, sizeof(client));
	char input[OD_LOGLINE_MAXLEN + 1];
	memset(input, '\n', sizeof(input) - 1);
	input[sizeof(input) - 1] = '\0';
	client.external_id = input;

	char expected[OD_LOGLINE_MAXLEN];
	for (size_t i = 0; i < sizeof(expected) - 2; i += 2) {
		expected[i] = '\\';
		expected[i + 1] = 'n';
	}
	expected[sizeof(expected) - 2] = '\n';
	test_logger_output(&client, "%x", "", expected, sizeof(expected) - 1);
	test_logger_output(&client, "%x%m", "ignored", expected,
			   sizeof(expected) - 1);
	test_logger_output(NULL, "%M", input, expected, sizeof(expected) - 1);
	expected[0] = '!';
	for (size_t i = 1; i < sizeof(expected) - 3; i += 2) {
		expected[i] = '\\';
		expected[i + 1] = 'n';
	}
	expected[sizeof(expected) - 3] = '\n';
	test_logger_output(&client, "!%x", "", expected, sizeof(expected) - 2);
}

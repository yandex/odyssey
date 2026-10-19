#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

static int read_error(const char *fields, size_t length, kiwi_fe_error_t *error)
{
	static char data[128];
	char *pos = data;
	kiwi_write8(&pos, KIWI_BE_ERROR_RESPONSE);
	kiwi_write32(&pos, sizeof(uint32_t) + length);
	kiwi_write(&pos, fields, length);
	return kiwi_fe_read_error(data, pos - data, error);
}

void kiwi_test_fe_read_error(void)
{
	kiwi_fe_error_t error;
	const char missing_portal[] = "C34000\0Rexec_execute_message\0";
	test(read_error(missing_portal, sizeof(missing_portal), &error) == 0);
	test(strcmp(error.code, "34000") == 0);
	test(error.routine != NULL);
	test(strcmp(error.routine, "exec_execute_message") == 0);

	/* An error inside a valid Execute must remain distinguishable. */
	const char query_error[] = "C34000\0Rexec_stmt_raise\0";
	test(read_error(query_error, sizeof(query_error), &error) == 0);
	test(strcmp(error.routine, "exec_stmt_raise") == 0);

	const char no_source[] = "C34000\0";
	test(read_error(no_source, sizeof(no_source), &error) == 0);
	test(error.routine == NULL);

	/* A source routine without a terminator is malformed. */
	const char truncated[] = "C34000\0Rexec_execute_message";
	test(read_error(truncated, sizeof(truncated) - 1, &error) == -1);
}

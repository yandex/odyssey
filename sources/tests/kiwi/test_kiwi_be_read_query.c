/*
 * Test for kiwi_be_read_query and kiwi_be_read_parse_dest payload
 * validation.
 *
 * A Query ('Q') message must carry a NUL-terminated query string and
 * a Parse ('P') message must carry a NUL-terminated query inside its
 * description.  Readers must reject messages without the terminator,
 * otherwise strlen-based callers read past the end of the message
 * buffer (heap-buffer-overflow read).
 */

#include <stdlib.h>
#include <string.h>
#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

/*
 * Helper to build a 'Q' message with exact-size heap allocation, so
 * out-of-bounds reads are caught by ASAN.
 * Format: 'Q' (1 byte) + length (4 bytes, big-endian) + payload
 */
static char *build_query_message(const char *payload, size_t payload_len,
				 uint32_t *total_len)
{
	char *buf = malloc(1 + 4 + payload_len);
	test(buf != NULL);

	buf[0] = KIWI_FE_QUERY;

	uint32_t len = 4 + payload_len;
	buf[1] = (len >> 24) & 0xFF;
	buf[2] = (len >> 16) & 0xFF;
	buf[3] = (len >> 8) & 0xFF;
	buf[4] = len & 0xFF;

	memcpy(buf + 5, payload, payload_len);

	*total_len = 1 + 4 + payload_len;
	return buf;
}

/*
 * Helper to build a 'P' message:
 * 'P' (1 byte) + length (4 bytes) + name\0 + query + int16 param count
 */
static char *build_parse_message(const char *query, size_t query_len,
				 uint32_t *total_len)
{
	size_t payload_len = 2 + query_len + 2;
	char *buf = malloc(1 + 4 + payload_len);
	test(buf != NULL);

	buf[0] = KIWI_FE_PARSE;

	uint32_t len = 4 + payload_len;
	buf[1] = (len >> 24) & 0xFF;
	buf[2] = (len >> 16) & 0xFF;
	buf[3] = (len >> 8) & 0xFF;
	buf[4] = len & 0xFF;

	/* operator_name: "s\0" */
	buf[5] = 's';
	buf[6] = '\0';

	memcpy(buf + 7, query, query_len);

	/* int16 parameter type count: 0 */
	buf[7 + query_len] = 0;
	buf[8 + query_len] = 0;

	*total_len = 1 + 4 + payload_len;
	return buf;
}

static void test_query_valid(void)
{
	uint32_t total_len;
	char *buf = build_query_message("SELECT 1\0", 9, &total_len);

	char *query = NULL;
	uint32_t query_len = 0;
	int rc = kiwi_be_read_query(buf, total_len, &query, &query_len);
	test(rc == 0);
	test(query == buf + 5);
	test(query_len == 9);
	test(strcmp(query, "SELECT 1") == 0);

	free(buf);
}

static void test_query_missing_nul(void)
{
	uint32_t total_len;
	/* no trailing NUL byte */
	char *buf = build_query_message("SELECT 1", 8, &total_len);

	char *query = NULL;
	uint32_t query_len = 0;
	int rc = kiwi_be_read_query(buf, total_len, &query, &query_len);
	test(rc == -1);

	free(buf);
}

static void test_query_empty_payload(void)
{
	uint32_t total_len;
	/* zero-length payload, no space for the terminator at all */
	char *buf = build_query_message("", 0, &total_len);

	char *query = NULL;
	uint32_t query_len = 0;
	int rc = kiwi_be_read_query(buf, total_len, &query, &query_len);
	test(rc == -1);

	free(buf);
}

static void test_parse_dest_valid(void)
{
	uint32_t total_len;
	char *buf = build_parse_message("SELECT 1\0", 9, &total_len);

	kiwi_prepared_statement_t dest;
	int rc = kiwi_be_read_parse_dest(buf, total_len, &dest);
	test(rc == 0);
	test(dest.operator_name_len == 2);
	test(strncmp(dest.operator_name, "s", 1) == 0);
	test(strncmp(dest.description, "SELECT 1", 8) == 0);

	free(buf);
}

static void test_parse_dest_missing_nul(void)
{
	uint32_t total_len;
	/* query without the trailing NUL inside the description */
	char *buf = build_parse_message("SELECT 1", 8, &total_len);

	kiwi_prepared_statement_t dest;
	int rc = kiwi_be_read_parse_dest(buf, total_len, &dest);
	test(rc == -1);

	free(buf);
}

void kiwi_test_be_read_query(void)
{
	test_query_valid();
	test_query_missing_nul();
	test_query_empty_payload();
	test_parse_dest_valid();
	test_parse_dest_missing_nul();
}

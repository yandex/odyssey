/*
 * Test for kiwi_fe_read_auth SASL mechanism list parsing.
 *
 * PostgreSQL sends a list of SASL mechanisms as null-terminated strings
 * in the AuthenticationSASL message. When TLS is enabled, PostgreSQL
 * sends: ["SCRAM-SHA-256-PLUS", "SCRAM-SHA-256", ""].
 *
 * This test verifies that the parser correctly handles:
 * 1. Single mechanism (SCRAM-SHA-256 only)
 * 2. Multiple mechanisms with SCRAM-SHA-256 first
 * 3. Multiple mechanisms with SCRAM-SHA-256-PLUS first (TLS case)
 * 4. Unsupported mechanisms only
 */

#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

/*
 * Helper to build an AuthenticationSASL message.
 * Format: 'R' (1 byte) + length (4 bytes, big-endian) + type (4 bytes) + mechanisms
 */
static void build_auth_sasl_message(char *buf, uint32_t *total_len,
				    const char *mechanisms, size_t mech_len)
{
	/* Message type */
	buf[0] = KIWI_BE_AUTHENTICATION;

	/* Length = 4 (length itself) + 4 (auth type) + mechanism data */
	uint32_t len = 4 + 4 + mech_len;
	buf[1] = (len >> 24) & 0xFF;
	buf[2] = (len >> 16) & 0xFF;
	buf[3] = (len >> 8) & 0xFF;
	buf[4] = len & 0xFF;

	/* Auth type = 10 (AuthenticationSASL) */
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 10;

	/* Mechanism list */
	memcpy(buf + 9, mechanisms, mech_len);

	*total_len = 1 + 4 + 4 + mech_len;
}

/* Test: Single SCRAM-SHA-256 mechanism (should succeed) */
void test_sasl_single_scram_sha_256(void)
{
	char buf[128];
	uint32_t total_len;

	/* "SCRAM-SHA-256\0" */
	const char mechanisms[] = "SCRAM-SHA-256";
	build_auth_sasl_message(buf, &total_len, mechanisms,
				sizeof(mechanisms));

	uint32_t type;
	char salt[4];
	char *auth_data;
	size_t auth_data_size;

	int rc = kiwi_fe_read_auth(buf, total_len, &type, salt, &auth_data,
				   &auth_data_size);
	test(rc == 0);
	test(type == 10);
}

/* Test: Multiple mechanisms with SCRAM-SHA-256 first (should succeed) */
void test_sasl_scram_sha_256_first(void)
{
	char buf[128];
	uint32_t total_len;

	/* "SCRAM-SHA-256\0SCRAM-SHA-256-PLUS\0\0" */
	const char mechanisms[] = "SCRAM-SHA-256\0SCRAM-SHA-256-PLUS\0";
	build_auth_sasl_message(buf, &total_len, mechanisms,
				sizeof(mechanisms));

	uint32_t type;
	char salt[4];
	char *auth_data;
	size_t auth_data_size;

	int rc = kiwi_fe_read_auth(buf, total_len, &type, salt, &auth_data,
				   &auth_data_size);
	test(rc == 0);
	test(type == 10);
}

/* Test: SCRAM-SHA-256-PLUS first, then SCRAM-SHA-256 (TLS case - should succeed) */
void test_sasl_scram_sha_256_plus_first(void)
{
	char buf[128];
	uint32_t total_len;

	/* "SCRAM-SHA-256-PLUS\0SCRAM-SHA-256\0\0" - this is what PostgreSQL sends with TLS */
	const char mechanisms[] = "SCRAM-SHA-256-PLUS\0SCRAM-SHA-256\0";
	build_auth_sasl_message(buf, &total_len, mechanisms,
				sizeof(mechanisms));

	uint32_t type;
	char salt[4];
	char *auth_data;
	size_t auth_data_size;

	int rc = kiwi_fe_read_auth(buf, total_len, &type, salt, &auth_data,
				   &auth_data_size);
	test(rc == 0);
	test(type == 10);
}

/* Test: Only unsupported mechanisms (should fail) */
void test_sasl_unsupported_only(void)
{
	char buf[128];
	uint32_t total_len;

	/* "SCRAM-SHA-512\0UNKNOWN\0\0" - no supported mechanism */
	const char mechanisms[] = "SCRAM-SHA-512\0UNKNOWN\0";
	build_auth_sasl_message(buf, &total_len, mechanisms,
				sizeof(mechanisms));

	uint32_t type;
	char salt[4];
	char *auth_data;
	size_t auth_data_size;

	int rc = kiwi_fe_read_auth(buf, total_len, &type, salt, &auth_data,
				   &auth_data_size);
	test(rc == -1);
}

/* Test: SCRAM-SHA-256-PLUS only without SCRAM-SHA-256 (should fail) */
void test_sasl_scram_sha_256_plus_only(void)
{
	char buf[128];
	uint32_t total_len;

	/* "SCRAM-SHA-256-PLUS\0\0" - channel binding only, no plain SCRAM */
	const char mechanisms[] = "SCRAM-SHA-256-PLUS\0";
	build_auth_sasl_message(buf, &total_len, mechanisms,
				sizeof(mechanisms));

	uint32_t type;
	char salt[4];
	char *auth_data;
	size_t auth_data_size;

	int rc = kiwi_fe_read_auth(buf, total_len, &type, salt, &auth_data,
				   &auth_data_size);
	test(rc == -1);
}

/* Test: Empty mechanism list (should fail) */
void test_sasl_empty_list(void)
{
	char buf[128];
	uint32_t total_len;

	/* "\0" - empty list */
	const char mechanisms[] = "";
	build_auth_sasl_message(buf, &total_len, mechanisms,
				sizeof(mechanisms));

	uint32_t type;
	char salt[4];
	char *auth_data;
	size_t auth_data_size;

	int rc = kiwi_fe_read_auth(buf, total_len, &type, salt, &auth_data,
				   &auth_data_size);
	test(rc == -1);
}

/* Test: SCRAM-SHA-256 as third mechanism (should succeed) */
void test_sasl_scram_sha_256_third(void)
{
	char buf[128];
	uint32_t total_len;

	/* "SCRAM-SHA-256-PLUS\0SCRAM-SHA-512\0SCRAM-SHA-256\0\0" */
	const char mechanisms[] =
		"SCRAM-SHA-256-PLUS\0SCRAM-SHA-512\0SCRAM-SHA-256\0";
	build_auth_sasl_message(buf, &total_len, mechanisms,
				sizeof(mechanisms));

	uint32_t type;
	char salt[4];
	char *auth_data;
	size_t auth_data_size;

	int rc = kiwi_fe_read_auth(buf, total_len, &type, salt, &auth_data,
				   &auth_data_size);
	test(rc == 0);
	test(type == 10);
}

/* Test: Malformed packet - no null terminator within bounds (should fail safely) */
void test_sasl_malformed_no_null_terminator(void)
{
	char buf[128];

	/* Build a malformed message manually - mechanism string without null */
	buf[0] = KIWI_BE_AUTHENTICATION;

	/* Length = 4 + 4 + 10 (no null terminator in mechanism) */
	uint32_t len = 4 + 4 + 10;
	buf[1] = (len >> 24) & 0xFF;
	buf[2] = (len >> 16) & 0xFF;
	buf[3] = (len >> 8) & 0xFF;
	buf[4] = len & 0xFF;

	/* Auth type = 10 */
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	buf[8] = 10;

	/* Mechanism data without null terminator - just fill with 'A' */
	memset(buf + 9, 'A', 10);

	uint32_t total_len = 1 + 4 + 4 + 10;

	uint32_t type;
	char salt[4];
	char *auth_data;
	size_t auth_data_size;

	/* Should fail safely without reading past buffer */
	int rc = kiwi_fe_read_auth(buf, total_len, &type, salt, &auth_data,
				   &auth_data_size);
	test(rc == -1);
}

void kiwi_test_fe_read_auth(void)
{
	test_sasl_single_scram_sha_256();
	test_sasl_scram_sha_256_first();
	test_sasl_scram_sha_256_plus_first();
	test_sasl_unsupported_only();
	test_sasl_scram_sha_256_plus_only();
	test_sasl_empty_list();
	test_sasl_scram_sha_256_third();
	test_sasl_malformed_no_null_terminator();
}

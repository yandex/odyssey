/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * Message length limits, checked against PostgreSQL:
 * src/backend/tcop/backend_startup.c (ProcessStartupPacket),
 * src/backend/tcop/postgres.c (SocketBackend),
 * src/backend/libpq/pqcomm.c (pq_getmessage),
 * src/backend/libpq/auth.c (PG_MAX_AUTH_TOKEN_LENGTH).
 */

#include <stdlib.h>
#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

static int validate_startup(uint32_t len)
{
	char header[4];
	uint32_t size;
	kiwi_write32to(header, len);
	return kiwi_validate_startup_header(header, sizeof(header), &size);
}

static int validate_fe(char type, uint32_t len)
{
	char header[5];
	uint32_t size;
	header[0] = type;
	kiwi_write32to(header + 1, len);
	return kiwi_validate_fe_header(header, sizeof(header), &size);
}

static int validate_be(char type, uint32_t len)
{
	char header[5];
	uint32_t size;
	header[0] = type;
	kiwi_write32to(header + 1, len);
	return kiwi_validate_be_header(header, sizeof(header), &size);
}

static int read_password(uint32_t len)
{
	char *packet = calloc(1, 1 + len);
	test(packet != NULL);
	packet[0] = KIWI_FE_PASSWORD_MESSAGE;
	kiwi_write32to(packet + 1, len);

	kiwi_password_t pw;
	kiwi_password_init(&pw);
	int rc = kiwi_be_read_password(packet, 1 + len, &pw);
	kiwi_password_free(&pw);
	free(packet);
	return rc;
}

/* the startup length word minus itself must not exceed 10000 */
static void test_startup_limit(void)
{
	test(validate_startup(8) == 0);
	test(validate_startup(PQ_SMALL_MESSAGE_LIMIT + 4) == 0);
	test(validate_startup(PQ_SMALL_MESSAGE_LIMIT + 5) == -1);
	test(validate_startup(3) == -1);
}

/* the length word, itself included, must not exceed the type's limit */
static void test_fe_limits(void)
{
	test(validate_fe(KIWI_FE_SYNC, 4) == 0);
	test(validate_fe(KIWI_FE_SYNC, 3) == -1);
	test(validate_fe(KIWI_FE_SYNC, PQ_SMALL_MESSAGE_LIMIT) == 0);
	test(validate_fe(KIWI_FE_SYNC, PQ_SMALL_MESSAGE_LIMIT + 1) == -1);
	test(validate_fe(KIWI_FE_QUERY, PQ_LARGE_MESSAGE_LIMIT) == 0);
	test(validate_fe(KIWI_FE_QUERY, PQ_LARGE_MESSAGE_LIMIT + 1) == -1);
}

static void test_be_limits(void)
{
	test(validate_be(KIWI_BE_DATA_ROW, PQ_LARGE_MESSAGE_LIMIT) == 0);
	test(validate_be(KIWI_BE_DATA_ROW, PQ_LARGE_MESSAGE_LIMIT + 1) == -1);
}

static void test_password_limit(void)
{
	test(read_password(PG_MAX_AUTH_TOKEN_LENGTH) == 0);
	test(read_password(PG_MAX_AUTH_TOKEN_LENGTH + 1) == -1);
}

void kiwi_test_msg_size_limits(void)
{
	test(PQ_LARGE_MESSAGE_LIMIT == 0x3fffffff - 1);
	test_startup_limit();
	test_fe_limits();
	test_be_limits();
	test_password_limit();
}

/*
 * kiwi.
 *
 * Cancellation message formats, checked against PostgreSQL 18.6:
 * src/backend/tcop/postgres.c (BackendKeyData),
 * src/backend/tcop/backend_startup.c (ProcessCancelRequestPacket),
 * src/interfaces/libpq/fe-cancel.c (PQsendCancelRequest).
 */

#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

static uint32_t build_backend_key_data(char *packet, uint32_t key_len)
{
	packet[0] = 'K';
	kiwi_write32to(packet + 1, 8 + key_len);
	kiwi_write32to(packet + 5, 0x12345678);
	for (uint32_t i = 0; i < key_len; i++) {
		packet[9 + i] = (char)(i * 73);
	}
	return 9 + key_len;
}

void kiwi_test_backend_key_data(void)
{
	char packet[9 + 257];
	const uint32_t lengths[] = { 4, 5, 32, 256 };
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
		uint32_t size = build_backend_key_data(packet, lengths[i]);
		kiwi_key_t key;
		kiwi_key_init(&key);
		test(kiwi_fe_read_key(packet, size, &key, PG_PROTOCOL(3, 2)) ==
		     0);
		test(key.key_pid == 0x12345678);
		test(key.key_len == lengths[i]);
		test(memcmp(key.key, packet + 9, key.key_len) == 0);
		/* libpq requires a four-byte key when using protocol 3.0. */
		test(kiwi_fe_read_key(packet, size, &key, PG_PROTOCOL(3, 0)) ==
		     (lengths[i] == 4 ? 0 : -1));
		test(kiwi_fe_read_key(packet, size - 1, &key,
				      PG_PROTOCOL(3, 2)) == -1);
	}

	const uint32_t invalid[] = { 0, 1, 3, 257 };
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		uint32_t size = build_backend_key_data(packet, invalid[i]);
		kiwi_key_t key;
		kiwi_key_init(&key);
		test(kiwi_fe_read_key(packet, size, &key, PG_PROTOCOL(3, 2)) ==
		     -1);
	}
	kiwi_key_t key;
	kiwi_key_init(&key);
	uint32_t size = build_backend_key_data(packet, 4);
	for (uint32_t i = 0; i < 9; i++) {
		test(kiwi_fe_read_key(packet, i, &key, PG_PROTOCOL(3, 2)) ==
		     -1);
	}
	packet[0] = 'R';
	test(kiwi_fe_read_key(packet, size, &key, PG_PROTOCOL(3, 2)) == -1);
}

static uint32_t build_cancel_request(char *packet, uint32_t key_len)
{
	kiwi_write32to(packet, 12 + key_len);
	kiwi_write32to(packet + 4, 80877102);
	kiwi_write32to(packet + 8, 0x12345678);
	for (uint32_t i = 0; i < key_len; i++) {
		packet[12 + i] = (char)(i * 73);
	}
	return 12 + key_len;
}

void kiwi_test_cancel_request(void)
{
	char packet[12 + 257];
	const uint32_t lengths[] = { 1, 3, 4, 5, 32, 256, 0, 257 };
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
		uint32_t size = build_cancel_request(packet, lengths[i]);
		kiwi_be_startup_t startup;
		kiwi_be_startup_init(&startup);
		kiwi_vars_t vars;
		kiwi_vars_init(&vars);
		int expected = lengths[i] > 0 && lengths[i] <= 256 ?
				       KIWI_STARTUP_READ_OK :
				       KIWI_STARTUP_CANCEL_READ_ERROR;
		test(kiwi_be_read_startup(packet, size, &startup, &vars) ==
		     expected);
		if (expected == KIWI_STARTUP_READ_OK) {
			test(startup.is_cancel);
			test(startup.key.key_pid == 0x12345678);
			test(startup.key.key_len == lengths[i]);
			test(memcmp(startup.key.key, packet + 12,
				    startup.key.key_len) == 0);
			/* Do not accept a packet shorter than its declared length. */
			test(kiwi_be_read_startup(packet, size - 1, &startup,
						  &vars) ==
			     KIWI_STARTUP_CANCEL_READ_ERROR);
		}
		kiwi_be_startup_free(&startup);
		kiwi_vars_free(&vars);
	}
	/* A truncated PID, empty key, and extra undeclared bytes are invalid. */
	for (uint32_t size = 8; size <= 12; size++) {
		build_cancel_request(packet, 4);
		kiwi_write32to(packet, size);
		kiwi_be_startup_t startup;
		kiwi_be_startup_init(&startup);
		kiwi_vars_t vars;
		kiwi_vars_init(&vars);
		test(kiwi_be_read_startup(packet, size, &startup, &vars) ==
		     KIWI_STARTUP_CANCEL_READ_ERROR);
		kiwi_be_startup_free(&startup);
		kiwi_vars_free(&vars);
	}
	uint32_t size = build_cancel_request(packet, 4);
	kiwi_be_startup_t startup;
	kiwi_be_startup_init(&startup);
	kiwi_vars_t vars;
	kiwi_vars_init(&vars);
	test(kiwi_be_read_startup(packet, size + 1, &startup, &vars) ==
	     KIWI_STARTUP_CANCEL_READ_ERROR);
	kiwi_be_startup_free(&startup);
	kiwi_vars_free(&vars);
}

static void test_cancel_write(void *arg)
{
	(void)arg;
	char expected[12 + 256];
	const uint32_t lengths[] = { 1, 3, 4, 5, 32, 256 };
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
		kiwi_key_t key;
		kiwi_key_init(&key);
		key.key_pid = 0x12345678;
		key.key_len = lengths[i];
		uint32_t size = build_cancel_request(expected, lengths[i]);
		memcpy(key.key, expected + 12, key.key_len);
		machine_msg_t *msg = kiwi_fe_write_cancel(NULL, &key);
		test(msg != NULL);
		test(machine_msg_size(msg) == (int)size);
		test(memcmp(machine_msg_data(msg), expected, size) == 0);
		machine_msg_free(msg);

		if (lengths[i] < 4) {
			test(kiwi_be_write_backend_key_data(NULL, &key) ==
			     NULL);
			continue;
		}
		size = build_backend_key_data(expected, lengths[i]);
		msg = kiwi_be_write_backend_key_data(NULL, &key);
		test(msg != NULL);
		test(machine_msg_size(msg) == (int)size);
		test(memcmp(machine_msg_data(msg), expected, size) == 0);
		machine_msg_free(msg);
	}

	kiwi_key_t key;
	kiwi_key_set32(&key, 0x12345678, 0x004992db);
	uint32_t size = build_cancel_request(expected, 4);
	machine_msg_t *msg = kiwi_fe_write_cancel(NULL, &key);
	test(msg != NULL);
	test(machine_msg_size(msg) == (int)size);
	test(memcmp(machine_msg_data(msg), expected, size) == 0);
	machine_msg_free(msg);
	test(kiwi_password_salt(&key, 0xabcdef01) ==
	     (0xabcdef01 ^ 0x004992db ^ 0x12345678));

	/* Invalid lengths must not change an existing output buffer. */
	msg = kiwi_be_write_ready(NULL, 'I');
	test(msg != NULL);
	for (uint32_t len = 0; len <= 257; len += 257) {
		key.key_len = len;
		test(kiwi_fe_write_cancel(msg, &key) == NULL);
		test(kiwi_be_write_backend_key_data(msg, &key) == NULL);
		test(machine_msg_size(msg) == 6);
	}
	machine_msg_free(msg);

	/* Writers must also work when appending to a non-empty stream. */
	kiwi_key_set32(&key, 0x12345678, 0x004992db);
	msg = kiwi_be_write_ready(NULL, 'I');
	test(msg != NULL);
	msg = kiwi_fe_write_cancel(msg, &key);
	test(msg != NULL);
	test(machine_msg_size(msg) == 22);
	size = build_cancel_request(expected, 4);
	test(memcmp((char *)machine_msg_data(msg) + 6, expected, size) == 0);
	msg = kiwi_be_write_backend_key_data(msg, &key);
	test(msg != NULL);
	test(machine_msg_size(msg) == 35);
	size = build_backend_key_data(expected, 4);
	test(memcmp((char *)machine_msg_data(msg) + 22, expected, size) == 0);
	machine_msg_free(msg);
}

void kiwi_test_cancel_write(void)
{
	test(machinarium_init() == 0);
	int64_t id = machine_create("cancel_write", test_cancel_write, NULL);
	test(id > 0);
	test(machine_wait(id) == 0);
	machinarium_free();
}

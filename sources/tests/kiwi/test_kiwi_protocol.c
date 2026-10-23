/*
 * kiwi.
 *
 * PostgreSQL protocol version negotiation tests.
 */

#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

static void test_protocol_negotiation(void *arg)
{
	(void)arg;
	/* Offer 3.2; an older backend can negotiate down to 3.0. */
	kiwi_fe_arg_t args[] = { { "user", 5 }, { "test", 5 } };
	const unsigned char startup[] = {
		0,   0,	  0,   19, /* length */
		0,   3,	  0,   2, /* maximum protocol version: 3.2 */
		'u', 's', 'e', 'r', 0, 't', 'e', 's', 't', 0, 0,
	};
	machine_msg_t *msg = kiwi_fe_write_startup_message(NULL, 2, args);
	test(msg != NULL);
	test(machine_msg_size(msg) == sizeof(startup));
	test(memcmp(machine_msg_data(msg), startup, sizeof(startup)) == 0);
	machine_msg_free(msg);

	/*
	 * PostgreSQL sends the full version; libpq expects the major bits too,
	 * despite the documentation describing this as the minor version.
	 */
	const unsigned char version_30[] = {
		'v', /* message type */
		0,   0, 0, 12, /* length */
		0,   3, 0, 0, /* protocol 3.0 */
		0,   0, 0, 0, /* unsupported option count */
	};
	msg = kiwi_be_write_protocol_negotiation(NULL, PG_PROTOCOL(3, 0), NULL,
						 0);
	test(msg != NULL);
	test(machine_msg_size(msg) == sizeof(version_30));
	test(memcmp(machine_msg_data(msg), version_30, sizeof(version_30)) ==
	     0);
	machine_msg_free(msg);

	const unsigned char version_32[] = {
		'v', /* message type */
		0,   0, 0, 12, /* length */
		0,   3, 0, 2, /* protocol 3.2 */
		0,   0, 0, 0, /* unsupported option count */
	};
	msg = kiwi_be_write_protocol_negotiation(NULL, PG_PROTOCOL(3, 2), NULL,
						 0);
	test(msg != NULL);
	test(machine_msg_size(msg) == sizeof(version_32));
	test(memcmp(machine_msg_data(msg), version_32, sizeof(version_32)) ==
	     0);
	machine_msg_free(msg);

	/* Appending negotiation must preserve the preceding message. */
	const char *unsupported[] = { "_pq_.a", "_pq_.b" };
	const unsigned char with_options[] = {
		'Z', 0,	  0,   0,   5,	 'I', /* preceding ReadyForQuery */
		'v', /* message type */
		0,   0,	  0,   26, /* length */
		0,   3,	  0,   2, /* protocol 3.2 */
		0,   0,	  0,   2, /* unsupported option count */
		'_', 'p', 'q', '_', '.', 'a', 0, /* first option */
		'_', 'p', 'q', '_', '.', 'b', 0, /* second option */
	};
	msg = kiwi_be_write_ready(NULL, 'I');
	test(msg != NULL);
	msg = kiwi_be_write_protocol_negotiation(msg, PG_PROTOCOL(3, 2),
						 unsupported, 2);
	test(msg != NULL);
	test(machine_msg_size(msg) == sizeof(with_options));
	test(memcmp(machine_msg_data(msg), with_options,
		    sizeof(with_options)) == 0);
	machine_msg_free(msg);
}

void kiwi_test_protocol_negotiation(void)
{
	test(machinarium_init() == 0);
	int64_t id = machine_create("protocol_negotiation",
				    test_protocol_negotiation, NULL);
	test(id > 0);
	test(machine_wait(id) == 0);
	machinarium_free();
}

void kiwi_test_startup_protocol_version(void)
{
	const struct {
		uint32_t version;
		int expected;
	} cases[] = {
		{ PG_PROTOCOL(3, 0), KIWI_STARTUP_READ_OK },
		{ PG_PROTOCOL(3, 2), KIWI_STARTUP_READ_OK },
		{ PG_PROTOCOL(3, 65535), KIWI_STARTUP_READ_OK },
		{ PG_PROTOCOL(2, 0), KIWI_STARTUP_INVALID_MAJOR_ERROR },
		{ PG_PROTOCOL(4, 0), KIWI_STARTUP_INVALID_MAJOR_ERROR },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char packet[] = {
			0,   0,	  0,   19, /* length */
			0,   0,	  0,   0, /* version, filled below */
			'u', 's', 'e', 'r', 0, /* parameter name */
			't', 'e', 's', 't', 0, /* parameter value */
			0, /* end of parameters */
		};
		kiwi_write32to(packet + 4, cases[i].version);
		kiwi_be_startup_t startup;
		kiwi_be_startup_init(&startup);
		kiwi_vars_t vars;
		kiwi_vars_init(&vars);

		/* New minor versions must reach negotiation, not be rejected. */
		test(kiwi_be_read_startup(packet, sizeof(packet), &startup,
					  &vars) == cases[i].expected);
		test(startup.proto_version == cases[i].version);

		kiwi_be_startup_free(&startup);
		kiwi_vars_free(&vars);
	}
}

void kiwi_test_read_protocol_negotiation(void)
{
	char packet[] = {
		'v', 0, 0, 0, 12, /* type and length */
		0,   3, 0, 0, /* selected protocol: 3.0 */
		0,   0, 0, 0, /* no unsupported options */
	};
	uint32_t version = PG_PROTOCOL(3, 2);
	test(kiwi_fe_read_protocol_negotiation(packet, sizeof(packet),
					       &version) == 0);
	test(version == PG_PROTOCOL(3, 0));
	/* A second negotiation cannot downgrade the connection again. */
	test(kiwi_fe_read_protocol_negotiation(packet, sizeof(packet),
					       &version) == -1);

	for (uint32_t size = 0; size < sizeof(packet); size++) {
		version = PG_PROTOCOL(3, 2);
		test(kiwi_fe_read_protocol_negotiation(packet, size,
						       &version) == -1);
		test(version == PG_PROTOCOL(3, 2));
	}

	const struct {
		uint32_t version;
		uint32_t unsupported;
	} invalid[] = {
		{ PG_PROTOCOL(2, 0), 0 },	   { PG_PROTOCOL(3, 1), 0 },
		{ PG_PROTOCOL(3, 2), 0 }, /* no changes requested */
		{ PG_PROTOCOL(3, 3), 0 },	   { PG_PROTOCOL(4, 0), 0 },
		{ PG_PROTOCOL(3, 0), 1 }, /* no extensions were requested */
		{ PG_PROTOCOL(3, 0), UINT32_MAX },
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		kiwi_write32to(packet + 5, invalid[i].version);
		kiwi_write32to(packet + 9, invalid[i].unsupported);
		version = PG_PROTOCOL(3, 2);
		test(kiwi_fe_read_protocol_negotiation(packet, sizeof(packet),
						       &version) == -1);
		test(version == PG_PROTOCOL(3, 2));
	}
	kiwi_write32to(packet + 5, PG_PROTOCOL(3, 0));
	kiwi_write32to(packet + 9, 0);
	const uint32_t lengths[] = { 0, 4, 11, 13, UINT32_MAX };
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
		kiwi_write32to(packet + 1, lengths[i]);
		version = PG_PROTOCOL(3, 2);
		test(kiwi_fe_read_protocol_negotiation(packet, sizeof(packet),
						       &version) == -1);
	}
	kiwi_write32to(packet + 1, 12);
	packet[0] = 'R';
	test(kiwi_fe_read_protocol_negotiation(packet, sizeof(packet),
					       &version) == -1);
	char trailing[sizeof(packet) + 1];
	memcpy(trailing, packet, sizeof(packet));
	trailing[0] = 'v';
	trailing[sizeof(packet)] = 0;
	test(kiwi_fe_read_protocol_negotiation(trailing, sizeof(trailing),
					       &version) == -1);
}

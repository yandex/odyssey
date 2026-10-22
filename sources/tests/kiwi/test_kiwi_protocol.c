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
	machine_msg_t *msg = kiwi_be_write_protocol_negotiation(
		NULL, PG_PROTOCOL(3, 0), NULL, 0);
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

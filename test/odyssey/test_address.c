#include "odyssey.h"
#include <odyssey_test.h>

typedef struct {
	char *host;
	char *zone;
	int port;
	od_address_type_t type;
} parse_test_arg_t;

static inline parse_test_arg_t address(char *h, char *z, int p)
{
	parse_test_arg_t r = {
		.host = h, .zone = z, .port = p, .type = OD_ADDRESS_TYPE_TCP
	};

	return r;
}

static inline parse_test_arg_t address_unix(char *h, char *z, int p)
{
	parse_test_arg_t r = {
		.host = h, .zone = z, .port = p, .type = OD_ADDRESS_TYPE_UNIX
	};

	return r;
}

static inline void do_test(const char *line, int expected_rc,
			   size_t expected_count, ...)
{
	od_address_t *addresses = NULL;
	size_t count = 0;

	int rc = od_parse_addresses(line, &addresses, &count);
	test(rc == expected_rc);
	test(count == expected_count);

	va_list args;
	va_start(args, expected_count);

	for (size_t i = 0; i < count; ++i) {
		parse_test_arg_t expected_endpoint =
			va_arg(args, parse_test_arg_t);

		test(strcmp(expected_endpoint.host, addresses[i].host) == 0);
		test(strcmp(expected_endpoint.zone,
			    addresses[i].availability_zone) == 0);
		test(expected_endpoint.port == addresses[i].port);
		test(expected_endpoint.type == addresses[i].type);

		free(addresses[i].host);
	}

	free(addresses);

	va_end(args);
}

void odyssey_test_address_parse(void)
{
	do_test("localhost", OK_RESPONSE, 1, address("localhost", "", 0));

	do_test("[localhost]", OK_RESPONSE, 1, address("localhost", "", 0));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net", OK_RESPONSE, 1,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "", 0));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:1337", OK_RESPONSE, 1,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "", 1337));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:1337:sas", OK_RESPONSE, 1,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "sas", 1337));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:sas", OK_RESPONSE, 1,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "sas", 0));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:sas:klg", NOT_OK_RESPONSE,
		0);

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:sas, sas-sokkge3ejever6ae.mdb.yandex.net:sas:klg",
		NOT_OK_RESPONSE, 0);

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:sas:1337", OK_RESPONSE, 1,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "sas", 1337));

	do_test("[::1]:sas:1337", OK_RESPONSE, 1, address("::1", "sas", 1337));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net,localhost,klg-kd19rbngltphuob9.mdb.yandex.net",
		OK_RESPONSE, 3,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "", 0),
		address("localhost", "", 0),
		address("klg-kd19rbngltphuob9.mdb.yandex.net", "", 0));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:1337,localhost,klg-kd19rbngltphuob9.mdb.yandex.net:31337",
		OK_RESPONSE, 3,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "", 1337),
		address("localhost", "", 0),
		address("klg-kd19rbngltphuob9.mdb.yandex.net", "", 31337));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:1337,[localhost],klg-kd19rbngltphuob9.mdb.yandex.net:31337",
		OK_RESPONSE, 3,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "", 1337),
		address("localhost", "", 0),
		address("klg-kd19rbngltphuob9.mdb.yandex.net", "", 31337));

	do_test("sas-sokkge3ejever6ae.mdb.yandex.net:1337:sas,localhost:5432:vla,klg-kd19rbngltphuob9.mdb.yandex.net:31337:klg",
		OK_RESPONSE, 3,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "sas", 1337),
		address("localhost", "vla", 5432),
		address("klg-kd19rbngltphuob9.mdb.yandex.net", "klg", 31337));

	do_test("[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:1337:sas,localhost:5432:vla,[2001:0db8:85a3:1234:5678:8a2e:1375:7334]:31337:klg",
		OK_RESPONSE, 3,
		address("2001:0db8:85a3:0000:0000:8a2e:0370:7334", "sas", 1337),
		address("localhost", "vla", 5432),
		address("2001:0db8:85a3:1234:5678:8a2e:1375:7334", "klg",
			31337));

	do_test("2001:0db8:85a3:0000:0000:8a2e:0370:7334:1337:sas,localhost:5432:vla,[2001:0db8:85a3:1234:5678:8a2e:1375:7334]:31337:klg",
		NOT_OK_RESPONSE, 0);

	do_test("tcp://sas-sokkge3ejever6ae.mdb.yandex.net:sas", OK_RESPONSE, 1,
		address("sas-sokkge3ejever6ae.mdb.yandex.net", "sas", 0));

	do_test("tcp://localhost:31337:klg", OK_RESPONSE, 1,
		address("localhost", "klg", 31337));

	do_test("tcp://127.9.12.34:31337:klg", OK_RESPONSE, 1,
		address("127.9.12.34", "klg", 31337));

	do_test("tcp://[2001:0db8:85a3:1234:5678:8a2e:1375:7334]:31337:klg",
		OK_RESPONSE, 1,
		address("2001:0db8:85a3:1234:5678:8a2e:1375:7334", "klg",
			31337));

	do_test("unix:///var/lib/postgresql/.s.5432", OK_RESPONSE, 1,
		address_unix("/var/lib/postgresql/.s.5432", "", 0));

	do_test("tcp://[2001:0db8:85a3:1234:5678:8a2e:1375:7334]:31337:klg,unix:///var/lib/postgresql/.s.5432",
		OK_RESPONSE, 2,
		address("2001:0db8:85a3:1234:5678:8a2e:1375:7334", "klg",
			31337),
		address_unix("/var/lib/postgresql/.s.5432", "", 0));
}

static inline int sign(int a)
{
	if (a > 0) {
		return 1;
	}

	if (a < 0) {
		return -1;
	}

	return 0;
}

static inline void do_cmp_test(parse_test_arg_t addr1, parse_test_arg_t addr2,
			       int expected)
{
	od_address_t a, b;
	od_address_init(&a);
	od_address_init(&b);

	a.type = addr1.type;
	strcpy(a.availability_zone, addr1.zone);
	a.host = addr1.host;
	a.port = addr1.port;

	b.type = addr2.type;
	strcpy(b.availability_zone, addr2.zone);
	b.host = addr2.host;
	b.port = addr2.port;

	int sut = od_address_cmp(&a, &b);
	test(sign(sut) == sign(expected));

	/* no od_address_destroy here */
}

void odyssey_test_address_cmp(void)
{
	do_cmp_test(address("localhost", "kl", 1337),
		    address_unix("localhost", "kl", 1337),
		    OD_ADDRESS_TYPE_TCP - OD_ADDRESS_TYPE_UNIX);

	do_cmp_test(address_unix("localhost", "kl", 1337),
		    address("localhost", "kl", 1337),
		    OD_ADDRESS_TYPE_UNIX - OD_ADDRESS_TYPE_TCP);

	do_cmp_test(address("localhost", "kl", 1337),
		    address("localhost", "kl", 1337), 0);

	do_cmp_test(address("localhost", "kl", 1338),
		    address("localhost", "kl", 1337), 1338 - 1337);

	do_cmp_test(address("localhost", "kl", 1337),
		    address("localhost", "kl", 1338), 1337 - 1338);

	do_cmp_test(address("localhost", "ko", 1337),
		    address("localhost", "kl", 1337), strcmp("ko", "kl"));

	do_cmp_test(address("localhost", "kl", 1337),
		    address("localhost", "ko", 1337), strcmp("kl", "ko"));

	do_cmp_test(address("127.0.0.1", "kl", 1337),
		    address("127.0.0.8", "kl", 1337),
		    strcmp("127.0.0.1", "127.0.0.8"));

	do_cmp_test(address("127.0.0.8", "kl", 1337),
		    address("127.0.0.1", "kl", 1337),
		    strcmp("127.0.0.8", "127.0.0.1"));

	do_cmp_test(address_unix("/var/lib/.s.5432", "", 0),
		    address_unix("/var/lib/.s.5432", "", 0), 0);

	do_cmp_test(address_unix("/var/lib/.s.5432", "dsfs", 0),
		    address_unix("/var/lib/.s.5432", "sdfgs", 0), 0);

	do_cmp_test(address_unix("/var/lib/.s.5432", "", 4131),
		    address_unix("/var/lib/.s.5432", "", 3134), 0);

	do_cmp_test(address_unix("/var/lib/.s.54322", "", 0),
		    address_unix("/var/lib/.s.54324", "", 0),
		    strcmp("/var/lib/.s.54322", "/var/lib/.s.54324"));

	do_cmp_test(address_unix("/var/lib/.s.54324", "", 0),
		    address_unix("/var/lib/.s.54322", "", 0),
		    strcmp("/var/lib/.s.54324", "/var/lib/.s.54322"));
}

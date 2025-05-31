#include "odyssey.h"
#include <odyssey_test.h>

typedef struct {
	const char *host;
	const char *zone;
	int port;
} parse_test_arg_t;

static inline parse_test_arg_t address(const char *h, const char *z, int p)
{
	parse_test_arg_t r = { .host = h, .zone = z, .port = p };

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
}
#include <odyssey.h>
#include <tests/odyssey_test.h>
#include <query_processing.h>

static void deallocate_test(const char *source, int ret, const char *name)
{
	const char *nret;
	size_t nretlen;
	int sut = od_parse_deallocate(source, strlen(source), &nret, &nretlen);
	test(ret == sut);
	if (ret == 0) {
		test(nretlen == strlen(name));
		test(strncmp(name, nret, nretlen) == 0);
	}
}

void odyssey_test_parse_deallocate(void)
{
	deallocate_test("DEALLOCATE foo", 0, "foo");
	deallocate_test("deallocate foo", 0, "foo");
	deallocate_test("DEALLOCATE foo;", 0, "foo");
	deallocate_test("  DEALLOCATE foo", 0, "foo");
	deallocate_test("\tDEALLOCATE\tfoo", 0, "foo");
	deallocate_test("DEALLOCATE    foo   ", 0, "foo");
	deallocate_test("DEALLOCATE foo   ;", 0, "foo");
	deallocate_test("DEALLOCATE foo;   ", 0, "foo");
	deallocate_test(" \n\r\t DEALLOCATE \n foo \r\n", 0, "foo");

	deallocate_test("DEALLOCATE PREPARE foo", 0, "foo");
	deallocate_test("deallocate prepare foo", 0, "foo");
	deallocate_test("DEALLOCATE PREPARE foo;", 0, "foo");
	deallocate_test("DEALLOCATE   PREPARE   foo", 0, "foo");
	deallocate_test("  DEALLOCATE PREPARE foo  ", 0, "foo");

	deallocate_test("DEALLOCATE ALL", 1, NULL);
	deallocate_test("deallocate all", 1, NULL);
	deallocate_test("DEALLOCATE ALL;", 1, NULL);
	deallocate_test("DEALLOCATE   ALL   ", 1, NULL);
	deallocate_test("  DEALLOCATE ALL  ; ", 1, NULL);
	deallocate_test("DEALLOCATE PREPARE ALL", 1, NULL);
	deallocate_test("DEALLOCATE PREPARE ALL;", 1, NULL);

	deallocate_test("DEALLOCATE \'foo\'", 0, "foo");
	deallocate_test("DEALLOCATE \'FooBar\'", 0, "FooBar");
	deallocate_test("DEALLOCATE PREPARE \'foo\'", 0, "foo");
	deallocate_test("DEALLOCATE \'foo\';", 0, "foo");
	deallocate_test("DEALLOCATE \'ALL\'", 0, "ALL");
	deallocate_test("DEALLOCATE PREPARE \'ALL\'", 0, "ALL");

	deallocate_test("DEALLOCATE \'a\'\'b\'", -1, NULL);
	deallocate_test("DEALLOCATE PREPARE \'a\'\'b\'", -1, NULL);
	deallocate_test("DEALLOCATE \'x\'\'y\'\'z\';", -1, NULL);

	deallocate_test("DEALLOCATE foo_bar", 0, "foo_bar");
	deallocate_test("DEALLOCATE foo123", 0, "foo123");

	deallocate_test("", -1, NULL);
	deallocate_test(" ", -1, NULL);
	deallocate_test("DEALLOCATE", -1, NULL);
	deallocate_test("DEALLOCATE ", -1, NULL);
	deallocate_test("DEALLOCATE\t", -1, NULL);
	deallocate_test("DEALLOCATE\n", -1, NULL);

	deallocate_test("DEALLOCATE PREPARE", -1, NULL);
	deallocate_test("DEALLOCATE PREPARE ", -1, NULL);
	deallocate_test("DEALLOCATE PREPARE;", -1, NULL);

	deallocate_test("ALLOCATE foo", -1, NULL);
	deallocate_test("PREPARE foo", -1, NULL);
	deallocate_test("SELECT 1", -1, NULL);

	deallocate_test("DEALLOCATE PREPARE pst", 0, "pst");
	deallocate_test("DEALLOCATE prepare pst", 0, "pst");

	deallocate_test("DEALLOCATE preparepst", 0, "preparepst");
	deallocate_test("DEALLOCATE PREPAREpst", 0, "PREPAREpst");

	deallocate_test("DEALLOCATE ALL", 1, NULL);
	deallocate_test("DEALLOCATE all", 1, NULL);
	deallocate_test("DEALLOCATE allpst", 0, "allpst");
	deallocate_test("DEALLOCATE ALLpst", 0, "ALLpst");

	deallocate_test("DEALLOCATE prepared pst", -1, NULL);
	deallocate_test("DEALLOCATE PREPARED pst", -1, NULL);

	deallocate_test("DEALLOCATE ALL pst", -1, NULL);
	deallocate_test("DEALLOCATE pst extra", -1, NULL);

	deallocate_test("DEALLOCATE \'ALL\'", 0, "ALL");
	deallocate_test("DEALLOCATE \'PREPARE\'", 0, "PREPARE");
}

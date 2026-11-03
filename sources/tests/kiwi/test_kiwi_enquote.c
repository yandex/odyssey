#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

void test_without_screen(void)
{
	char *simple_string = "abcdefg123098adad[]";
	size_t sz = 25;
	char dst[sz];
	int res;
	res = kiwi_enquote(simple_string, dst, sz);
	test(res != -1);
	test(strcmp(dst, "E\'abcdefg123098adad[]\'") == 0)
}

void test_one_screen(void)
{
	char *string = "root\\bin";
	size_t sz = 15;
	char dst[sz];
	int res;
	res = kiwi_enquote(string, dst, sz);
	test(res != -1);
	test(strcmp(dst, "E\'root\\\\bin\'") == 0)
}

void test_many_screens(void)
{
	char *string = "\\root\\bin\\\'easy\'win\'";
	size_t sz = 100;
	char dst[sz];
	int res;
	res = kiwi_enquote(string, dst, sz);
	test(res != -1);
	test(strcmp(dst, "E\'\\\\root\\\\bin\\\\\'\'easy\'\'win\'\'\'") == 0)
}

void test_invalid(void)
{
	char *string = "\\root\\bin\\\'easy\'win\'";
	size_t sz = 10;
	char dst[sz];
	int res;
	res = kiwi_enquote(string, dst, sz);
	test(res == -1);
}

void test_exact_buffer(void)
{
	/* An exact-sized destination includes the trailing NUL. */
	char exact[7];
	test(kiwi_enquote("abc", exact, sizeof(exact)) == 6);
	test(strcmp(exact, "E'abc'") == 0);
	test(kiwi_enquote("a'", exact, sizeof(exact)) == 6);
	test(strcmp(exact, "E'a'''") == 0);
	test(kiwi_enquote("a'", exact, sizeof(exact) - 1) == -1);
}

void kiwi_test_enquote(void)
{
	test_without_screen();
	test_one_screen();
	test_many_screens();
	test_invalid();
	test_exact_buffer();
}

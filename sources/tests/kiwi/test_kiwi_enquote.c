#include <kiwi/kiwi.h>
#include <tests/odyssey_test.h>

void test_without_screen()
{
	char *simple_string = "abcdefg123098adad[]";
	size_t sz = 25;
	char dst[sz];
	int res;
	res = kiwi_enquote(simple_string, dst, sz);
	test(res != -1);
	test(strcmp(dst, "E\'abcdefg123098adad[]\'") == 0)
}

void test_one_screen()
{
	char *string = "root\\bin";
	size_t sz = 15;
	char dst[sz];
	int res;
	res = kiwi_enquote(string, dst, sz);
	test(res != -1);
	test(strcmp(dst, "E\'root\\\\bin\'") == 0)
}

void test_many_screens()
{
	char *string = "\\root\\bin\\\'easy\'win\'";
	size_t sz = 100;
	char dst[sz];
	int res;
	res = kiwi_enquote(string, dst, sz);
	test(res != -1);
	test(strcmp(dst, "E\'\\\\root\\\\bin\\\\\'\'easy\'\'win\'\'\'") == 0)
}

void test_invalid()
{
	char *string = "\\root\\bin\\\'easy\'win\'";
	size_t sz = 10;
	char dst[sz];
	int res;
	res = kiwi_enquote(string, dst, sz);
	test(res == -1);
}

void kiwi_test_enquote(void)
{
	test_without_screen();
	test_one_screen();
	test_many_screens();
	test_invalid();
}

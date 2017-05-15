
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

extern void test_init(void);
extern void test_create(void);

extern void test_sleep(void);
extern void test_sleep_yield(void);
extern void test_sleep_cancel0(void);
extern void test_sleep_cancel1(void);

int
main(int argc, char *argv[])
{
	machinarium_test(test_init);
	machinarium_test(test_create);
	machinarium_test(test_sleep);
	machinarium_test(test_sleep_yield);
	machinarium_test(test_sleep_cancel0);
	machinarium_test(test_sleep_cancel1);
	return 0;
}

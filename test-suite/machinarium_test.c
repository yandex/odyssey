
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

extern void test_init(void);

int
main(int argc, char *argv[])
{
	machinarium_test(test_init);
	return 0;
}

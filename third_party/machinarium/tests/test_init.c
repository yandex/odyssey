
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

void
test_init(void)
{
	machinarium_init();
	machinarium_free();
}

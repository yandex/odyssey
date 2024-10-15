#include <machinarium.h>
#include <odyssey_test.h>

void machinarium_test_advice_keepalive_usr_timeout(void)
{
	test(machine_advice_keepalive_usr_timeout(100, 10, 3) == 129);
	test(machine_advice_keepalive_usr_timeout(15, 10, 5) == 64);
}
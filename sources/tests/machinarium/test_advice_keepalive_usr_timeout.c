#include <machinarium/machinarium.h>
#include <tests/odyssey_test.h>

void machinarium_test_advice_keepalive_usr_timeout(void)
{
	test(machine_advice_keepalive_usr_timeout(100, 10, 3) == 129500);
	test(machine_advice_keepalive_usr_timeout(15, 10, 5) == 64500);
}

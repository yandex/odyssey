#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

void machinarium_test_advice_keepalive_usr_timeout(void)
{
	test(mm_io_advice_keepalive_usr_timeout(100, 10, 3) == 129500);
	test(mm_io_advice_keepalive_usr_timeout(15, 10, 5) == 64500);
}

#include <odyssey.h>
#include <restart_sync.h>
#include <tests/odyssey_test.h>

void test_odyssey_locks_has_noneq_hash()
{
	test(ODYSSEY_CTRL_LOCK_HASH == 1337);
	test(ODYSSEY_CTRL_LOCK_HASH != ODYSSEY_EXEC_LOCK_HASH);
}

void odyssey_test_lock(void)
{
	test_odyssey_locks_has_noneq_hash();
}

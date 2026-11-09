
#include <machinarium/machinarium.h>
#include <tests/odyssey_test.h>
#include <stdatomic.h>

/*
 * machine_reserve_ids() + machine_create_at(): a block of machine ids
 * is reserved atomically, machines created inside the block get the
 * reserved ids, the next auto-assigned id continues after the block.
 */

static atomic_uint_fast64_t test_done;

static inline void reserve_test_machine_main(void *arg)
{
	(void)arg;
	atomic_fetch_add(&test_done, 1);
}

static inline void reserve_test_root(void *arg)
{
	(void)arg;

	/* reserve a block of two ids */
	int64_t base;
	base = machine_reserve_ids(2);
	test(base != -1);

	/* create machines inside the block */
	int64_t id1;
	id1 = machine_create_at((uint64_t)base + 0, "reserved1",
				reserve_test_machine_main, NULL);
	test(id1 == base + 0);

	int64_t id2;
	id2 = machine_create_at((uint64_t)base + 1, "reserved2",
				reserve_test_machine_main, NULL);
	test(id2 == base + 1);

	/* wait for both machines to finish */
	uint64_t deadline = machine_time_ms() + 5000;
	while (atomic_load(&test_done) < 2) {
		test(machine_time_ms() < deadline);
		machine_sleep(5);
	}

	int rc;
	rc = machine_wait((uint64_t)id1);
	test(rc == 0);
	rc = machine_wait((uint64_t)id2);
	test(rc == 0);

	/* the next auto-assigned id continues after the block */
	int64_t id3;
	id3 = machine_create("after_block", reserve_test_machine_main, NULL);
	test(id3 == base + 2);

	atomic_fetch_add(&test_done, 1);

	rc = machine_wait((uint64_t)id3);
	test(rc == 0);
}

void machinarium_test_machine_reserve_ids(void)
{
	machinarium_init();

	atomic_store(&test_done, 0);

	int id;
	id = machine_create("test_reserve_ids", reserve_test_root, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}

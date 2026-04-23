
#include <machinarium/machinarium.h>
#include <machinarium/cond.h>
#include <machinarium/wait_flag.h>
#include <tests/odyssey_test.h>

static machine_cond_t *condition = NULL;

static void test_condition_coroutine(void *arg)
{
	(void)arg;
	machine_cond_signal(condition);
	machine_stop_current();
}

static void test_waiter(void *arg)
{
	(void)arg;

	condition = machine_cond_create();
	test(condition != NULL);

	int64_t a;
	a = machine_coroutine_create(test_condition_coroutine, NULL);
	test(a != -1);

	int rc;
	rc = machine_cond_wait(condition, UINT32_MAX);
	test(rc == 0);

	machine_stop_current();
}

static void test_simple(void)
{
	int id;
	id = machine_create("test", test_waiter, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);
}

atomic_uint_fast64_t success_counter;
atomic_uint_fast64_t timeout_counter;

static void awaiter_success(void *arg)
{
	mm_cond_t *cond = arg;

	int rc = mm_cond_wait(cond, UINT32_MAX);
	test(rc == 0);

	atomic_fetch_add(&success_counter, 1);
}

typedef struct {
	mm_cond_t *cond;
	mm_wait_flag_t *end_flag;
} timeout_arg_t;

static void awaiter_timeout(void *arg)
{
	timeout_arg_t *a = arg;
	mm_cond_t *cond = a->cond;

	int rc = mm_cond_wait(cond, 100);
	test(rc == -1 && machine_errno() == ETIMEDOUT);

	atomic_fetch_add(&timeout_counter, 1);

	mm_wait_flag_set(a->end_flag);
}

static void several_awaiters(void *a)
{
	(void)a;

	mm_cond_t cond;
	mm_cond_init(&cond);

	atomic_init(&success_counter, 0);

	int64_t a1;
	a1 = machine_coroutine_create(awaiter_success, &cond);
	test(a1 != -1);
	machine_sleep(0);

	int64_t a2;
	a2 = machine_coroutine_create(awaiter_success, &cond);
	test(a2 != -1);
	machine_sleep(0);

	int64_t a3;
	a3 = machine_coroutine_create(awaiter_success, &cond);
	test(a3 != -1);
	machine_sleep(0);

	mm_cond_signal(&cond);

	int rc = machine_join(a1);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));
	rc = machine_join(a2);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));
	rc = machine_join(a3);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));

	test(atomic_load(&success_counter) == 3);
}

static void test_several_awaiters(void)
{
	int id;
	id = machine_create("several_awaiter", several_awaiters, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);
}

static void several_awaiters_timeout(void *a)
{
	(void)a;

	mm_cond_t cond;
	mm_cond_init(&cond);

	atomic_init(&success_counter, 0);
	atomic_init(&timeout_counter, 0);

	timeout_arg_t targ;
	targ.cond = &cond;
	targ.end_flag = mm_wait_flag_create();

	int64_t a1;
	a1 = machine_coroutine_create(awaiter_timeout, &targ);
	test(a1 != -1);
	machine_sleep(0);

	int64_t a2;
	a2 = machine_coroutine_create(awaiter_success, &cond);
	test(a2 != -1);
	machine_sleep(0);

	int64_t a3;
	a3 = machine_coroutine_create(awaiter_success, &cond);
	test(a3 != -1);
	machine_sleep(0);

	mm_wait_flag_wait(targ.end_flag, UINT32_MAX);

	mm_cond_signal(&cond);

	int rc = machine_join(a1);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));
	rc = machine_join(a2);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));
	rc = machine_join(a3);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));

	test(atomic_load(&success_counter) == 2);
	test(atomic_load(&timeout_counter) == 1);

	mm_wait_flag_destroy(targ.end_flag);
}

static void test_several_awaiters_timeout(void)
{
	int id;
	id = machine_create("several_awaiter", several_awaiters_timeout, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);
}

atomic_uint_fast64_t propagated_counter;

static void awaiter_propagated(void *arg)
{
	mm_cond_t *cond = arg;

	int rc = mm_cond_wait(cond, UINT32_MAX);
	test(rc == MM_COND_WAIT_OK_PROPAGATED);

	atomic_fetch_add(&propagated_counter, 1);
}

static void several_awaiters_propagated(void *a)
{
	(void)a;

	mm_cond_t src, dst;
	mm_cond_init(&src);
	mm_cond_init(&dst);
	mm_cond_propagate(&src, &dst);

	atomic_init(&propagated_counter, 0);

	int64_t a1 = machine_coroutine_create(awaiter_propagated, &dst);
	test(a1 != -1);
	machine_sleep(0);

	int64_t a2 = machine_coroutine_create(awaiter_propagated, &dst);
	test(a2 != -1);
	machine_sleep(0);

	int64_t a3 = machine_coroutine_create(awaiter_propagated, &dst);
	test(a3 != -1);
	machine_sleep(0);

	mm_cond_signal(&src);

	int rc = machine_join(a1);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));
	rc = machine_join(a2);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));
	rc = machine_join(a3);
	test(rc == 0 || (rc == -1 && machine_errno() == ENOENT));

	test(atomic_load(&propagated_counter) == 3);
}

static void test_several_awaiters_propagated(void)
{
	int id;
	id = machine_create("several_awaiter_propagated",
			    several_awaiters_propagated, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);
}

void machinarium_test_condition0(void)
{
	machinarium_init();

	test_simple();

	test_several_awaiters();

	test_several_awaiters_timeout();

	test_several_awaiters_propagated();

	machinarium_free();
}

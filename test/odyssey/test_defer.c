#include "odyssey.h"

typedef struct {
	int called;
} some_arg;

void deferrer(void *arg)
{
	some_arg *a = (some_arg *)arg;
	a->called++;
}

void wrapper(some_arg *arg)
{
	od_defer(deferrer, (void *)arg);

	assert(!arg->called);
}

static void test_called_after_function_return()
{
	some_arg arg;
	memset(&arg, 0, sizeof(arg));

	wrapper(&arg);

	assert(arg.called);
}

static void test_called_after_block_out()
{
	some_arg arg;
	memset(&arg, 0, sizeof(arg));

	{
		od_defer(deferrer, (void *)&arg);

		assert(!arg.called);
	}

	assert(arg.called);
}

static void test_several_defers()
{
	some_arg arg;
	memset(&arg, 0, sizeof(arg));

	{
		od_defer(deferrer, (void *)&arg);
		od_defer(deferrer, (void *)&arg);
	}

	assert(arg.called == 2);
}

static void wrapper_2(some_arg *arg)
{
	od_defer(deferrer, (void *)arg);

	if (arg->called) {
		assert(!arg->called);
		return;
	}

	assert(!arg->called);
}

static void test_on_additional_return()
{
	some_arg arg;
	memset(&arg, 0, sizeof(arg));

	wrapper_2(&arg);
	assert(arg.called);

	wrapper_2(&arg);
	assert(arg.called == 2);
}

void odyssey_test_defer()
{
	test_called_after_function_return();
	test_called_after_block_out();
	test_several_defers();
}
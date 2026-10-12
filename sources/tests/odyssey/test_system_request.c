#include <odyssey.h>
#include <machinarium/machine.h>
#include <global.h>
#include <system.h>
#include <msg.h>
#include <tests/odyssey_test.h>

typedef struct {
	od_system_request_t *request;
	mm_wait_flag_t *entered;
} canceled_waiter_t;

static void canceled_waiter(void *arg)
{
	canceled_waiter_t *waiter = arg;
	mm_wait_flag_set(waiter->entered);
	test(od_system_request_wait(waiter->request) == -1);
	od_system_request_unref(waiter->request);
}

static od_system_request_t *read_request(od_system_t *system)
{
	machine_msg_t *msg = machine_channel_read(system->system_channel,
						  UINT32_MAX);
	test(msg != NULL);
	test(machine_msg_type(msg) == OD_MSG_RELOAD);
	od_system_request_t *request;
	memcpy(&request, machine_msg_data(msg), sizeof(request));
	machine_msg_free(msg);
	return request;
}

static void request_consumer(void *arg)
{
	od_system_t *system = arg;
	for (int i = 0; i < 100; i++) {
		od_system_request_unref(read_request(system));
	}
}

static void test_request_lifetime(void *arg)
{
	(void)arg;
	od_system_t *system = od_system_create();
	test(system != NULL);
	od_global_t global = { 0 };
	system->global = &global;
	system->system_channel = machine_channel_create();
	test(system->system_channel != NULL);

	canceled_waiter_t waiter;
	waiter.request = od_system_request_create(false);
	waiter.entered = mm_wait_flag_create();
	test(waiter.request != NULL && waiter.entered != NULL);
	test(od_system_request_submit(system, OD_MSG_RELOAD,
				      waiter.request) == 0);
	int64_t coro = machine_coroutine_create(canceled_waiter, &waiter);
	test(coro != -1);
	test(mm_wait_flag_wait(waiter.entered, UINT32_MAX) == 0);
	machine_cancel(coro);
	machine_join(coro);
	mm_wait_flag_destroy(waiter.entered);
	/* The canceled coroutine released its reference. The queued message
	 * still owns a live request, which the system can safely release. */
	test(read_request(system) == waiter.request);
	od_system_request_unref(waiter.request);

	/* Fire-and-forget SIGHUP has the same ownership, without a waiter. */
	od_system_request_t *request = od_system_request_create(true);
	test(request != NULL);
	test(od_system_request_submit(system, OD_MSG_RELOAD, request) == 0);
	od_system_request_unref(request);
	test(read_request(system) == request);
	od_system_request_unref(request);

	/* Race releases from the caller and another OS thread. */
	int64_t consumer = machine_create("request-consumer", request_consumer,
					  system);
	test(consumer != -1);
	for (int i = 0; i < 100; i++) {
		request = od_system_request_create(false);
		test(request != NULL);
		test(od_system_request_submit(system, OD_MSG_RELOAD,
					      request) == 0);
		od_system_request_unref(request);
	}
	test(machine_wait(consumer) == 0);

	od_system_requests_close(system);
	request = od_system_request_create(false);
	test(request != NULL);
	test(od_system_request_submit(system, OD_MSG_RELOAD, request) == -1);
	od_system_request_unref(request);
	test(machine_channel_read(system->system_channel, 0) == NULL);
	machine_channel_free(system->system_channel);
	od_system_free(system);
}

void odyssey_test_system_request(void)
{
	test(machinarium_init() == 0);
	int64_t machine = machine_create("request-test", test_request_lifetime,
					 NULL);
	test(machine != -1);
	test(machine_wait(machine) == 0);
	machinarium_free();
}

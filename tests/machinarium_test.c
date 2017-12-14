
/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_test.h>

extern void test_init(void);
extern void test_create0(void);
extern void test_create1(void);
extern void test_context_switch(void);

extern void test_sleep(void);
extern void test_sleep_yield(void);
extern void test_sleep_cancel0(void);

extern void test_join(void);
extern void test_condition0(void);
extern void test_condition1(void);

extern void test_signal0(void);
extern void test_signal1(void);
extern void test_signal2(void);

extern void test_channel_create(void);
extern void test_channel_rw0(void);
extern void test_channel_rw1(void);
extern void test_channel_rw2(void);
extern void test_channel_rw3(void);
extern void test_channel_rw4(void);
extern void test_channel_timeout(void);
extern void test_channel_cancel(void);

extern void test_queue_create(void);
extern void test_queue_rw0(void);
extern void test_queue_rw1(void);
extern void test_queue_rw2(void);
extern void test_queue_producer_consumer0(void);
extern void test_queue_producer_consumer1(void);

extern void test_io_new(void);
extern void test_connect(void);
extern void test_connect_timeout(void);
extern void test_connect_cancel0(void);
extern void test_connect_cancel1(void);
extern void test_accept_timeout(void);
extern void test_accept_cancel(void);

extern void test_getaddrinfo0(void);
extern void test_getaddrinfo1(void);
extern void test_getaddrinfo2(void);

extern void test_client_server0(void);
extern void test_client_server1(void);

extern void test_read_10mb0(void);
extern void test_read_10mb1(void);
extern void test_read_10mb2(void);
extern void test_read_timeout(void);
extern void test_read_cancel(void);

extern void test_read_poll0(void);
extern void test_read_poll1(void);
extern void test_read_poll2(void);
extern void test_read_poll3(void);

extern void test_tls0(void);
extern void test_tls_read_10mb0(void);
extern void test_tls_read_10mb1(void);
extern void test_tls_read_10mb2(void);
extern void test_tls_read_10mb_poll(void);

int
main(int argc, char *argv[])
{
	machinarium_test(test_init);
	machinarium_test(test_create0);
	machinarium_test(test_create1);
	machinarium_test(test_sleep);
	machinarium_test(test_context_switch);
	machinarium_test(test_sleep_yield);
	machinarium_test(test_sleep_cancel0);
	machinarium_test(test_join);
	machinarium_test(test_condition0);
	machinarium_test(test_condition1);
	machinarium_test(test_signal0);
	machinarium_test(test_signal1);
	machinarium_test(test_signal2);
	machinarium_test(test_channel_create);
	machinarium_test(test_channel_rw0);
	machinarium_test(test_channel_rw1);
	machinarium_test(test_channel_rw2);
	machinarium_test(test_channel_rw3);
	machinarium_test(test_channel_rw4);
	machinarium_test(test_channel_timeout);
	machinarium_test(test_channel_cancel);
	machinarium_test(test_queue_create);
	machinarium_test(test_queue_rw0);
	machinarium_test(test_queue_rw1);
	machinarium_test(test_queue_rw2);
	machinarium_test(test_queue_producer_consumer0);
	machinarium_test(test_queue_producer_consumer1);
	machinarium_test(test_io_new);
	machinarium_test(test_connect);
	machinarium_test(test_connect_timeout);
	machinarium_test(test_connect_cancel0);
	machinarium_test(test_connect_cancel1);
	machinarium_test(test_accept_timeout);
	machinarium_test(test_accept_cancel);
	machinarium_test(test_getaddrinfo0);
	machinarium_test(test_getaddrinfo1);
	machinarium_test(test_getaddrinfo2);
	machinarium_test(test_client_server0);
	machinarium_test(test_client_server1);
	machinarium_test(test_read_10mb0);
	machinarium_test(test_read_10mb1);
	machinarium_test(test_read_10mb2);
	machinarium_test(test_read_timeout);
	machinarium_test(test_read_cancel);
	machinarium_test(test_read_poll0);
	machinarium_test(test_read_poll1);
	machinarium_test(test_read_poll2);
	machinarium_test(test_read_poll3);
	machinarium_test(test_tls0);
	machinarium_test(test_tls_read_10mb0);
	machinarium_test(test_tls_read_10mb1);
	machinarium_test(test_tls_read_10mb2);
	machinarium_test(test_tls_read_10mb_poll);
	return 0;
}

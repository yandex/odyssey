
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

#include <arpa/inet.h>
#include <sys/socket.h>

static void server(void *arg)
{
	int reset = *(int *)arg;
	mm_io_t *listener = mm_io_create();
	test(listener != NULL);
	struct sockaddr_in sa = { 0 };
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	test(mm_io_bind(listener, (struct sockaddr *)&sa,
			MM_BINDWITH_SO_REUSEADDR) == 0);
	mm_io_t *peer;
	test(mm_io_accept(listener, &peer, 16, 1, UINT32_MAX) == 0);
	machine_sleep(50);
	if (reset) {
		struct linger linger = { .l_onoff = 1, .l_linger = 0 };
		test(setsockopt(mm_io_fd(peer), SOL_SOCKET, SO_LINGER, &linger,
				sizeof(linger)) == 0);
	}
	test(mm_io_close(peer) == 0);
	mm_io_free(peer);
	test(mm_io_close(listener) == 0);
	mm_io_free(listener);
}

static void client(void *arg)
{
	int reset = *(int *)arg;
	mm_io_t *io = mm_io_create();
	test(io != NULL);
	struct sockaddr_in sa = { 0 };
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	test(mm_io_connect(io, (struct sockaddr *)&sa, UINT32_MAX) == 0);
	/* Let the event loop consume SO_ERROR before attempting recv. The
	 * latched reset must still be distinguishable from a clean EOF. */
	for (int i = 0; io->connected && i < 100; i++) {
		machine_sleep(10);
	}
	test(!io->connected);
	if (reset) {
		test(io->error == ECONNRESET);
		/* A poll batch can report one reset through both read and write
		 * filters. SO_ERROR was consumed by the first notification. */
		io->handle.on_err(&io->handle);
		test(io->error == ECONNRESET);
	}
	machine_msg_t *msg = machine_read(io, 1, 500);
	test(msg == NULL);
	test(machine_errno() == (reset ? ECONNRESET : 0));
	test(mm_io_close(io) == 0);
	mm_io_free(io);
}

static void test_cs(void *arg)
{
	test(machine_coroutine_create(server, arg) != -1);
	test(machine_coroutine_create(client, arg) != -1);
}

void machinarium_test_read_close(void)
{
	for (int reset = 0; reset <= 1; reset++) {
		machinarium_init();
		int id = machine_create("test", test_cs, &reset);
		test(id != -1);
		test(machine_wait(id) != -1);
		machinarium_free();
	}
}

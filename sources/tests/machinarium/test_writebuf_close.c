
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * close(): a buffered layer must not lose the data it holds -
 * mm_io_close() performs a single best-effort flush before
 * closing the fd, on a stall the rest is discarded.
 */

#define BUF_SIZE 8192

static void fill(char *buf, size_t size, int phase)
{
	size_t i;
	for (i = 0; i < size; i++) {
		buf[i] = (char)((i * 31 + phase) & 0xff);
	}
}

static void check(machine_msg_t *msg, size_t size, int phase)
{
	char *expect = malloc(size);
	test(expect != NULL);
	fill(expect, size, phase);
	test(memcmp(machine_msg_data(msg), expect, size) == 0);
	machine_msg_free(msg);
	free(expect);
}

/* tests are machinarium-level users: loop the single-attempt flush */
static void test_flush(mm_io_t *io)
{
	mm_io_set_deadline(io, UINT32_MAX);
	int rc;
	while ((rc = mm_io_flush(io)) == -1) {
		test(machine_errno_retryable(machine_errno()));
		rc = mm_io_wait_deadline(io);
		test(rc != MM_COND_WAIT_FAIL);
	}
	test(rc == 0);
	test(mm_io_write_pending(io) == 0);
}

static void server(void *arg)
{
	(void)arg;
	mm_io_t *server = mm_io_create();
	test(server != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	int rc;
	rc = mm_io_bind(server, (struct sockaddr *)&sa,
			MM_BINDWITH_SO_REUSEADDR);
	test(rc == 0);

	mm_io_t *client;
	rc = mm_io_accept(server, &client, 16, 1, UINT32_MAX);
	test(rc == 0);
	mm_io_set_nodelay(client, 1);

	/* first batch: flushed explicitly by the client */
	machine_msg_t *msg;
	msg = machine_read(client, 300, UINT32_MAX);
	test(msg != NULL);
	check(msg, 300, 1);

	/*
	 * second batch: written into the buffer and sent by
	 * mm_io_close() without an explicit flush
	 */
	msg = machine_read(client, 300, UINT32_MAX);
	test(msg != NULL);
	check(msg, 300, 2);

	/* and then eof */
	msg = machine_read(client, 300, UINT32_MAX);
	test(msg == NULL);

	rc = mm_io_close(client);
	test(rc == 0);
	mm_io_free(client);

	rc = mm_io_close(server);
	test(rc == 0);
	mm_io_free(server);
}

static void client(void *arg)
{
	(void)arg;
	mm_io_t *client = mm_io_create();
	test(client != NULL);
	mm_io_set_nodelay(client, 1);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	int rc;
	rc = mm_io_connect(client, (struct sockaddr *)&sa, UINT32_MAX);
	test(rc == 0);

	rc = mm_io_buf_enable(client, BUF_SIZE);
	test(rc == 0);
	test(mm_io_write_pending(client) == 0);

	char buf[300];
	fill(buf, sizeof(buf), 1);
	rc = machine_write_raw(client, buf, sizeof(buf), NULL);
	test(rc == (ssize_t)sizeof(buf));
	test_flush(client);

	/* buffered but never flushed explicitly */
	fill(buf, sizeof(buf), 2);
	rc = machine_write_raw(client, buf, sizeof(buf), NULL);
	test(rc == (ssize_t)sizeof(buf));
	test(mm_io_write_pending(client) == 300);

	/* the close itself must push the buffered batch out */
	rc = mm_io_close(client);
	test(rc == 0);
	mm_io_free(client);
}

static void test_cs(void *arg)
{
	(void)arg;
	int rc;
	rc = machine_coroutine_create(server, NULL);
	test(rc != -1);

	rc = machine_coroutine_create(client, NULL);
	test(rc != -1);
}

void machinarium_test_writebuf_close(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_cs, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}

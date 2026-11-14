
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 8192

static void fill(char *buf, size_t size, int phase)
{
	size_t i;
	for (i = 0; i < size; i++) {
		buf[i] = (char)((i * 31 + phase) & 0xff);
	}
}

static void fill_at(char *buf, size_t size, size_t offset)
{
	size_t i;
	for (i = 0; i < size; i++) {
		buf[i] = (char)(((offset + i) * 31 + 4) & 0xff);
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
	rc = mm_io_set_compression(client, 'f');
	test(rc == 0);

	/* small writes must be buffered until an explicit flush */
	machine_msg_t *msg;
	msg = machine_read(client, 300, 100);
	test(msg == NULL);

	msg = machine_read(client, 300, UINT32_MAX);
	test(msg != NULL);
	check(msg, 300, 1);

	/* large write: block-aligned part bypasses the buf layer */
	msg = machine_read(client, 64 * 1024 + 500, UINT32_MAX);
	test(msg != NULL);
	check(msg, 64 * 1024 + 500, 2);

	/* writev passthrough */
	msg = machine_read(client, 32 * 1024, UINT32_MAX);
	test(msg != NULL);
	check(msg, 32 * 1024, 4);

	/* ack so that the client can drain and close cleanly */
	msg = machine_msg_create(0);
	test(msg != NULL);
	char ack = 'A';
	rc = machine_msg_write(msg, &ack, 1);
	test(rc == 0);
	rc = machine_write(client, msg, UINT32_MAX);
	test(rc == 0);

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

	/* compression attaches before the buf layer */
	rc = mm_io_set_compression(client, 'f');
	test(rc == 0);
	rc = mm_io_buf_enable(client, BUF_SIZE);
	test(rc == 0);
	test(mm_io_write_pending(client) == 0);

	char buf[300];
	fill(buf, sizeof(buf), 1);
	rc = machine_write_raw(client, buf, 100, NULL);
	test(rc == 100);
	rc = machine_write_raw(client, buf + 100, 100, NULL);
	test(rc == 100);
	rc = machine_write_raw(client, buf + 200, 100, NULL);
	test(rc == 100);

	/* give the server time to hit the timed read above */
	machine_sleep(300);
	test_flush(client);

	char *big = malloc(64 * 1024 + 500);
	test(big != NULL);
	fill(big, 64 * 1024 + 500, 2);
	size_t off = 0;
	while (off < 64 * 1024 + 500) {
		rc = machine_write_raw(client, big + off, 64 * 1024 + 500 - off,
				       NULL);
		test(rc > 0);
		off += (size_t)rc;
	}
	test(off == 64 * 1024 + 500);
	test_flush(client);
	free(big);

	char *part0 = malloc(16 * 1024);
	char *part1 = malloc(16 * 1024);
	test(part0 != NULL);
	test(part1 != NULL);
	fill_at(part0, 16 * 1024, 0);
	fill_at(part1, 16 * 1024, 16 * 1024);
	struct iovec iov[2];
	iov[0].iov_base = part0;
	iov[0].iov_len = 16 * 1024;
	iov[1].iov_base = part1;
	iov[1].iov_len = 16 * 1024;
	rc = machine_writev_raw(client, iov, 2, NULL);
	test(rc == (ssize_t)(32 * 1024));
	test_flush(client);
	free(part0);
	free(part1);

	/* wait for the server ack, then eof */
	machine_msg_t *msg;
	msg = machine_read(client, 1, UINT32_MAX);
	test(msg != NULL);
	test(machine_msg_size(msg) == 1);
	machine_msg_free(msg);

	msg = machine_read(client, 1, UINT32_MAX);
	test(msg == NULL);

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

void machinarium_test_writebuf_zpq(void)
{
	if (machine_compression_choose_alg("fz") == MM_ZPQ_NO_COMPRESSION) {
		/* no compression backend available in this build */
		return;
	}

	machinarium_init();

	int id;
	id = machine_create("test", test_cs, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}

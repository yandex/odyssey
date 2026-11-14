
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

	/* small writes must be buffered until an explicit flush */
	machine_msg_t *msg;
	msg = machine_read(client, 300, 100);
	test(msg == NULL);

	msg = machine_read(client, 300, UINT32_MAX);
	test(msg != NULL);
	check(msg, 300, 1);

	/* large write: block-aligned part bypasses the buffer */
	msg = machine_read(client, 64 * 1024 + 500, UINT32_MAX);
	test(msg != NULL);
	check(msg, 64 * 1024 + 500, 2);

	/* many small writes overflowing the buffer */
	msg = machine_read(client, 40 * 1024, UINT32_MAX);
	test(msg != NULL);
	check(msg, 40 * 1024, 3);

	/* writev */
	msg = machine_read(client, 600, UINT32_MAX);
	test(msg != NULL);
	check(msg, 600, 4);

	/* big writev: passthrough (the buffer is empty) */
	msg = machine_read(client, 32 * 1024, UINT32_MAX);
	test(msg != NULL);
	char *expect = malloc(32 * 1024);
	test(expect != NULL);
	fill_at(expect, 32 * 1024, 600);
	test(memcmp(machine_msg_data(msg), expect, 32 * 1024) == 0);
	machine_msg_free(msg);
	free(expect);

	/* the client disabled the buf layer and wrote the next 100
	 * bytes directly - read them, otherwise the close() below
	 * would reset the connection and discard the ack */
	msg = machine_read(client, 100, UINT32_MAX);
	test(msg != NULL);
	check(msg, 100, 1);

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
	test(mm_io_write_pending(client) == 300);

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
	test(mm_io_write_pending(client) == 500);
	test_flush(client);
	free(big);

	int total = 40 * 1024;
	char *small = malloc(total);
	test(small != NULL);
	fill(small, total, 3);
	size_t done = 0;
	while (done < (size_t)total) {
		rc = machine_write_raw(client, small + done, 1024, NULL);
		test(rc > 0);
		done += (size_t)rc;
	}
	test(done == (size_t)total);
	test(mm_io_write_pending(client) <= BUF_SIZE);
	test_flush(client);
	free(small);

	char part0[200];
	char part1[100];
	char part2[300];
	fill_at(part0, sizeof(part0), 0);
	fill_at(part1, sizeof(part1), 200);
	fill_at(part2, sizeof(part2), 300);
	struct iovec iov[3];
	iov[0].iov_base = part0;
	iov[0].iov_len = sizeof(part0);
	iov[1].iov_base = part1;
	iov[1].iov_len = sizeof(part1);
	iov[2].iov_base = part2;
	iov[2].iov_len = sizeof(part2);
	rc = machine_writev_raw(client, iov, 3, NULL);
	test(rc == (ssize_t)(sizeof(part0) + sizeof(part1) + sizeof(part2)));
	test(mm_io_write_pending(client) == 600);
	test_flush(client);

	/* big writev: passthrough, nothing is buffered */
	char *wv0 = malloc(16 * 1024);
	char *wv1 = malloc(16 * 1024);
	test(wv0 != NULL);
	test(wv1 != NULL);
	fill_at(wv0, 16 * 1024, 600);
	fill_at(wv1, 16 * 1024, 600 + 16 * 1024);
	struct iovec wv[2];
	wv[0].iov_base = wv0;
	wv[0].iov_len = 16 * 1024;
	wv[1].iov_base = wv1;
	wv[1].iov_len = 16 * 1024;
	rc = machine_writev_raw(client, wv, 2, NULL);
	test(rc == (ssize_t)(32 * 1024));
	test_flush(client);
	test(mm_io_write_pending(client) == 0);
	free(wv0);
	free(wv1);

	/* attach-order guards: the buf layer must stay on top */
	rc = mm_io_set_compression(client, 'f');
	test(rc == -1);
	test(machine_errno() == EINPROGRESS);

	/* disable and check that writes go through directly */
	rc = mm_io_buf_disable(client);
	test(rc == 0);
	rc = machine_write_raw(client, buf, 100, NULL);
	test(rc == 100);
	test(mm_io_write_pending(client) == 0);

	/* re-enable/disable cycle: the slot must be reusable and the
	 * buffer state released */
	rc = mm_io_buf_enable(client, BUF_SIZE);
	test(rc == 0);
	rc = mm_io_buf_disable(client);
	test(rc == 0);
	rc = mm_io_buf_enable(client, BUF_SIZE);
	test(rc == 0);
	rc = mm_io_buf_disable(client);
	test(rc == 0);

	/* tls on top of compression would silently skip it; the check
	 * runs only when a compression backend is actually available */
#ifdef MM_BUILD_COMPRESSION
	if (machine_compression_choose_alg("f") != MM_ZPQ_NO_COMPRESSION) {
		mm_io_t *guard = mm_io_create();
		test(guard != NULL);
		rc = mm_io_set_compression(guard, 'f');
		test(rc == 0);
		machine_tls_t *tls;
		tls = machine_tls_create();
		test(tls != NULL);
		rc = mm_io_set_tls(guard, tls, 0);
		test(rc == -1);
		test(machine_errno() == EINPROGRESS);
		machine_tls_free(tls);
		/* the io was never connected - nothing to close */
		mm_io_free(guard);
	}
#endif

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

void machinarium_test_writebuf(void)
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

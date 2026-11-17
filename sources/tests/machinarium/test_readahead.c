
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * readahead: the read area of the buf layer pulls more data from the
 * stack than the caller asked for (glibc stdio style), the excess is
 * served by the following reads without touching the lower layer.
 */

#define RA_SIZE 8192

static void fill(char *buf, size_t size, int phase)
{
	size_t i;
	for (i = 0; i < size; i++) {
		buf[i] = (char)((i * 31 + phase) & 0xff);
	}
}

static void fill_at(char *buf, size_t size, size_t offset, int phase)
{
	size_t i;
	for (i = 0; i < size; i++) {
		buf[i] = (char)(((offset + i) * 31 + phase) & 0xff);
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

static void check_at(machine_msg_t *msg, size_t size, size_t offset, int phase)
{
	char *expect = malloc(size);
	test(expect != NULL);
	fill_at(expect, size, offset, phase);
	test(memcmp(machine_msg_data(msg), expect, size) == 0);
	machine_msg_free(msg);
	free(expect);
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

	/* enable the readahead, the write area stays off */
	rc = mm_io_buf_enable(client, 0, RA_SIZE);
	test(rc == 0);
	test(mm_io_read_pending(client) == 0);

	/* no data yet - the read times out */
	machine_msg_t *msg;
	msg = machine_read(client, 300, 100);
	test(msg == NULL);

	/* a small read pulls the whole write into the readahead,
	 * the excess is served by the following reads */
	msg = machine_read(client, 100, UINT32_MAX);
	test(msg != NULL);
	check_at(msg, 100, 0, 1);
	test(mm_io_read_pending(client) == 200);
	test(mm_io_read_pending(client) != 0);

	msg = machine_read(client, 100, UINT32_MAX);
	test(msg != NULL);
	check_at(msg, 100, 100, 1);
	test(mm_io_read_pending(client) == 100);

	msg = machine_read(client, 100, UINT32_MAX);
	test(msg != NULL);
	check_at(msg, 100, 200, 1);
	test(mm_io_read_pending(client) == 0);
	test(mm_io_read_pending(client) == 0);

	/* many small reads are served by full refills */
	int total = 40 * 1024;
	size_t done = 0;
	while (done < (size_t)total) {
		size_t chunk = 1000;
		if (done + chunk > (size_t)total) {
			chunk = (size_t)total - done;
		}
		msg = machine_read(client, chunk, UINT32_MAX);
		test(msg != NULL);
		check_at(msg, chunk, done, 2);
		done += chunk;
	}
	test(mm_io_read_pending(client) == 0);

	/* large read: the block-aligned part bypasses the buffer */
	msg = machine_read(client, 64 * 1024 + 500, UINT32_MAX);
	test(msg != NULL);
	check(msg, 64 * 1024 + 500, 3);
	test(mm_io_read_pending(client) == 0);

	/* the layer cannot be disabled while the readahead holds
	 * unread data */
	msg = machine_read(client, 100, UINT32_MAX);
	test(msg != NULL);
	check_at(msg, 100, 0, 4);
	test(mm_io_read_pending(client) == 200);

	rc = mm_io_buf_disable(client);
	test(rc == -1);
	test(machine_errno() == EINPROGRESS);

	msg = machine_read(client, 200, UINT32_MAX);
	test(msg != NULL);
	check_at(msg, 200, 100, 4);
	test(mm_io_read_pending(client) == 0);

	rc = mm_io_buf_disable(client);
	test(rc == 0);
	test(mm_io_read_pending(client) == 0);

	/* attach guards on a throwaway io */
	mm_io_t *guard = mm_io_create();
	test(guard != NULL);

	/* at least one direction must be enabled */
	rc = mm_io_buf_enable(guard, 0, 0);
	test(rc == -1);
	test(machine_errno() == EINVAL);

	/* the layer cannot be attached twice */
	rc = mm_io_buf_enable(guard, RA_SIZE, 0);
	test(rc == 0);
	rc = mm_io_buf_enable(guard, RA_SIZE, 0);
	test(rc == -1);
	test(machine_errno() == EINPROGRESS);

	/* compression must go below the buffered layer */
	rc = mm_io_set_compression(guard, 'f');
	test(rc == -1);
	test(machine_errno() == EINPROGRESS);

	/* tls must go below the buffered layer: after the handshake
	 * SSL_read/SSL_write bypass the stack below the TLS layer */
	machine_tls_t *tls = machine_tls_create();
	test(tls != NULL);
	rc = mm_io_set_tls(guard, tls, 0);
	test(rc == -1);
	test(machine_errno() == EINPROGRESS);
	machine_tls_free(tls);

	rc = mm_io_buf_disable(guard);
	test(rc == 0);

	/* re-enable cycle: the slot must be reusable and the buffer
	 * state released */
	rc = mm_io_buf_enable(guard, 0, RA_SIZE);
	test(rc == 0);
	rc = mm_io_buf_disable(guard);
	test(rc == 0);
	rc = mm_io_buf_enable(guard, 0, RA_SIZE);
	test(rc == 0);
	rc = mm_io_buf_disable(guard);
	test(rc == 0);

	/* the io was never connected - nothing to close */
	mm_io_free(guard);

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

	/* give the server time to hit the timed read above */
	machine_sleep(300);

	char buf[300];
	fill(buf, sizeof(buf), 1);
	rc = machine_write_raw(client, buf, sizeof(buf), NULL);
	test(rc == (ssize_t)sizeof(buf));

	/* give the server time to consume the phase */
	machine_sleep(300);

	char *small = malloc(40 * 1024);
	test(small != NULL);
	fill_at(small, 40 * 1024, 0, 2);
	size_t off = 0;
	while (off < 40 * 1024) {
		size_t chunk = 1024;
		if (off + chunk > 40 * 1024) {
			chunk = 40 * 1024 - off;
		}
		rc = machine_write_raw(client, small + off, chunk, NULL);
		test(rc > 0);
		off += (size_t)rc;
	}
	test(off == 40 * 1024);
	free(small);
	machine_sleep(300);

	char *big = malloc(64 * 1024 + 500);
	test(big != NULL);
	fill(big, 64 * 1024 + 500, 3);
	off = 0;
	while (off < 64 * 1024 + 500) {
		rc = machine_write_raw(client, big + off, 64 * 1024 + 500 - off,
				       NULL);
		test(rc > 0);
		off += (size_t)rc;
	}
	free(big);
	machine_sleep(300);

	fill(buf, sizeof(buf), 4);
	rc = machine_write_raw(client, buf, sizeof(buf), NULL);
	test(rc == (ssize_t)sizeof(buf));

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

void machinarium_test_readahead(void)
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

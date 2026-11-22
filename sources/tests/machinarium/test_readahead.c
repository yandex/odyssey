
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

static void check_vec(struct iovec vec, size_t offset, int phase)
{
	char *expect = malloc(vec.iov_len);
	test(expect != NULL);
	fill_at(expect, vec.iov_len, offset, phase);
	test(memcmp(vec.iov_base, expect, vec.iov_len) == 0);
	free(expect);
}

/*
 * refill until data arrives: the cond wait can be woken up by a
 * signal left from an earlier io event, while the peer has not sent
 * anything yet
 */
static ssize_t refill_wait(mm_io_t *io)
{
	for (;;) {
		ssize_t n = mm_io_buf_try_refill(io);
		if (n != -1 || machine_errno() != EAGAIN) {
			return n;
		}
		mm_io_set_deadline(io, UINT32_MAX);
		test(mm_io_wait_deadline(io) != MM_COND_WAIT_FAIL);
	}
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

	/* peek/refill with no read area */
	ssize_t n;
	struct iovec gvec = mm_io_buf_peek(guard);
	test(gvec.iov_len == 0);
	n = mm_io_buf_try_refill(guard);
	test(n == -1);
	test(machine_errno() == EAGAIN);

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

	/* peek/refill without the buf layer */
	gvec = mm_io_buf_peek(guard);
	test(gvec.iov_len == 0);
	mm_io_buf_peek_commit(guard, 0);
	n = mm_io_buf_try_refill(guard);
	test(n == -1);
	test(machine_errno() == EINVAL);

	/* the io was never connected - nothing to close */
	mm_io_free(guard);

	/*
	 * peek/commit/refill: the readahead was disabled above, attach
	 * it again and walk the next client chunk through the peeked
	 * view; the buffered bytes always form a single contiguous run
	 */
	rc = mm_io_buf_enable(client, 0, RA_SIZE);
	test(rc == 0);

	struct iovec vec = mm_io_buf_peek(client);
	test(vec.iov_len == 0);

	size_t got = 0;
	while (got < 300) {
		n = refill_wait(client);
		test(n > 0);
		vec = mm_io_buf_peek(client);
		test(vec.iov_len == (size_t)n);
		check_vec(vec, got, 5);
		mm_io_buf_peek_commit(client, vec.iov_len);
		got += vec.iov_len;
		/* the read area itself must be empty, the kernel socket
		 * buffer may still hold the undelivered tail */
		vec = mm_io_buf_peek(client);
		test(vec.iov_len == 0);
	}
	test(got == 300);

	/* let the client proceed to the next chunk */
	msg = machine_msg_create(0);
	test(msg != NULL);
	char ack_b = 'B';
	rc = machine_msg_write(msg, &ack_b, 1);
	test(rc == 0);
	rc = machine_write(client, msg, UINT32_MAX);
	test(rc == 0);

	/*
	 * a small read area: the refill fills it to capacity, further
	 * refills report ENOBUFS, the space freed by peek_commit is
	 * appended to the remaining bytes and the view stays contiguous
	 */
	rc = mm_io_buf_disable(client);
	test(rc == 0);
	rc = mm_io_buf_enable(client, 0, 64);
	test(rc == 0);

	/* the refill must fill the small area up to its capacity */
	while (mm_io_buf_peek(client).iov_len < 64) {
		n = refill_wait(client);
		test(n > 0);
	}
	vec = mm_io_buf_peek(client);
	test(vec.iov_len == 64);
	check_vec(vec, 0, 6);

	n = mm_io_buf_try_refill(client);
	test(n == -1);
	test(machine_errno() == ENOBUFS);

	mm_io_buf_peek_commit(client, 32);
	vec = mm_io_buf_peek(client);
	test(vec.iov_len == 32);
	check_vec(vec, 32, 6);

	n = mm_io_buf_try_refill(client);
	test(n == 32);
	vec = mm_io_buf_peek(client);
	test(vec.iov_len == 64);
	check_vec(vec, 32, 6);
	mm_io_buf_peek_commit(client, 64);
	vec = mm_io_buf_peek(client);
	test(vec.iov_len == 0);

	/* the rest of the chunk goes through the same small area */
	got = 0;
	while (got < 304) {
		n = refill_wait(client);
		test(n > 0);
		vec = mm_io_buf_peek(client);
		test(vec.iov_len == (size_t)n);
		check_vec(vec, 96 + got, 6);
		mm_io_buf_peek_commit(client, vec.iov_len);
		got += vec.iov_len;
	}
	test(got == 304);
	vec = mm_io_buf_peek(client);
	test(vec.iov_len == 0);

	/* ack so that the client can drain and close cleanly */
	msg = machine_msg_create(0);
	test(msg != NULL);
	char ack = 'A';
	rc = machine_msg_write(msg, &ack, 1);
	test(rc == 0);
	rc = machine_write(client, msg, UINT32_MAX);
	test(rc == 0);

	/*
	 * eof: the peer closes, the refill reports it by returning 0
	 * with no errno set
	 */
	for (;;) {
		n = mm_io_buf_try_refill(client);
		if (n == 0) {
			break;
		}
		test(n == -1);
		test(machine_errno() == EAGAIN);
		mm_io_set_deadline(client, UINT32_MAX);
		test(mm_io_wait_deadline(client) != MM_COND_WAIT_FAIL);
	}
	test(machine_errno() == 0);

	rc = mm_io_buf_disable(client);
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

	/*
	 * extra chunks for the peek/refill section: the server acks
	 * the first one before the second is sent, so the server side
	 * walk through the peeked view is deterministic
	 */
	machine_sleep(300);
	fill(buf, sizeof(buf), 5);
	rc = machine_write_raw(client, buf, sizeof(buf), NULL);
	test(rc == (ssize_t)sizeof(buf));

	machine_msg_t *ack;
	ack = machine_read(client, 1, UINT32_MAX);
	test(ack != NULL);
	test(machine_msg_size(ack) == 1);
	machine_msg_free(ack);

	machine_sleep(300);
	char extra[400];
	fill_at(extra, sizeof(extra), 0, 6);
	rc = machine_write_raw(client, extra, sizeof(extra), NULL);
	test(rc == (ssize_t)sizeof(extra));

	/*
	 * wait for the server ack, then close: the server checks its
	 * eof handling on the refill after the peer is gone
	 */
	machine_msg_t *msg;
	msg = machine_read(client, 1, UINT32_MAX);
	test(msg != NULL);
	test(machine_msg_size(msg) == 1);
	machine_msg_free(msg);

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

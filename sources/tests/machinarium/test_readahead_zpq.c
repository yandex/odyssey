
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * readahead on top of the compression layer: the read area is filled
 * by the decompressor, the excess plaintext is served by the
 * following reads.
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

	/* the readahead attaches on top of the compression layer,
	 * the write area stays off */
	rc = mm_io_buf_enable(client, 0, RA_SIZE);
	test(rc == 0);
	test(mm_io_read_pending(client) == 0);

	/* no data yet - the read times out */
	machine_msg_t *msg;
	msg = machine_read(client, 300, 100);
	test(msg == NULL);

	/* a small read pulls the decompressed data into the
	 * readahead, the excess is served by the following reads */
	msg = machine_read(client, 100, UINT32_MAX);
	test(msg != NULL);
	check_at(msg, 100, 0, 1);
	test(mm_io_read_pending(client) == 200);

	msg = machine_read(client, 100, UINT32_MAX);
	test(msg != NULL);
	check_at(msg, 100, 100, 1);
	test(mm_io_read_pending(client) == 100);

	msg = machine_read(client, 100, UINT32_MAX);
	test(msg != NULL);
	check_at(msg, 100, 200, 1);
	/*
	 * no drain-to-empty check here: the compressed transport
	 * delivers flush tails asynchronously, the pending count is
	 * not deterministic until the peer goes quiescent and the
	 * whole frame is consumed; the plain test covers it
	 */

	/* large read: the block-aligned part bypasses the buffer */
	msg = machine_read(client, 64 * 1024 + 500, UINT32_MAX);
	test(msg != NULL);
	check(msg, 64 * 1024 + 500, 2);
	test(mm_io_read_pending(client) == 0);

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

	/* give the server time to hit the timed read above */
	machine_sleep(300);

	char buf[300];
	fill(buf, sizeof(buf), 1);
	rc = machine_write_raw(client, buf, sizeof(buf), NULL);
	test(rc == (ssize_t)sizeof(buf));
	/* drain the compressor tx tail, the block alone is not the
	 * whole frame */
	test_flush(client);

	/* give the server time to consume the phase */
	machine_sleep(300);

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
	free(big);
	test_flush(client);

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

void machinarium_test_readahead_zpq(void)
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


#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define COMPRESSION_PAYLOAD_SIZE (64 * 1024)
#define COMPRESSION_PORT 7801

static char compression_algorithm;
static char compression_payload[COMPRESSION_PAYLOAD_SIZE];
static char compression_reply[COMPRESSION_PAYLOAD_SIZE];

static struct iovec iov_advance(struct iovec *iov, int cnt, size_t size)
{
	while (cnt > 0) {
		if (iov->iov_len > size) {
			iov->iov_base = (char *)iov->iov_base + size;
			iov->iov_len -= size;
			break;
		}
		size -= iov->iov_len;
		iov++;
		cnt--;
	}
	struct iovec ret;
	ret.iov_base = iov;
	ret.iov_len = cnt;
	return ret;
}

static void server(void *arg)
{
	(void)arg;
	mm_io_t *server = mm_io_create();
	test(server != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(COMPRESSION_PORT);
	int rc;
	rc = mm_io_bind(server, (struct sockaddr *)&sa,
			MM_BINDWITH_SO_REUSEADDR);
	test(rc == 0);

	mm_io_t *client;
	rc = mm_io_accept(server, &client, 16, 1, UINT32_MAX);
	test(rc == 0);

	if (compression_algorithm != MM_ZPQ_NO_COMPRESSION) {
		rc = mm_io_set_compression(client, compression_algorithm);
		test(rc == 0);
	}

	machine_msg_t *msg;
	msg = machine_read(client, sizeof("ping"), UINT32_MAX);
	test(msg != NULL);
	test(memcmp(machine_msg_data(msg), "ping", sizeof("ping")) == 0);
	machine_msg_free(msg);

	msg = machine_read(client, COMPRESSION_PAYLOAD_SIZE, UINT32_MAX);
	test(msg != NULL);
	test(memcmp(machine_msg_data(msg), compression_payload,
		    COMPRESSION_PAYLOAD_SIZE) == 0);
	machine_msg_free(msg);

	/* reply with a writev of three chunks, retrying on partial
	 * progress like od_io_writev does */
	struct iovec chunks[3];
	chunks[0].iov_base = compression_reply;
	chunks[0].iov_len = 100;
	chunks[1].iov_base = compression_reply + 100;
	chunks[1].iov_len = COMPRESSION_PAYLOAD_SIZE / 2;
	chunks[2].iov_base =
		compression_reply + 100 + COMPRESSION_PAYLOAD_SIZE / 2;
	chunks[2].iov_len =
		COMPRESSION_PAYLOAD_SIZE - 100 - COMPRESSION_PAYLOAD_SIZE / 2;
	struct iovec *iov = chunks;
	int iovcnt = 3;
	size_t total = 0;
	while (iovcnt > 0) {
		size_t processed = 0;
		ssize_t rc =
			machine_writev_raw(client, iov, iovcnt, &processed);
		size_t consumed = processed + (rc > 0 ? (size_t)rc : 0);
		total += consumed;
		if (consumed > 0) {
			struct iovec rest = iov_advance(iov, iovcnt, consumed);
			iov = rest.iov_base;
			iovcnt = rest.iov_len;
		}
		if (rc > 0) {
			continue;
		}
		int errno_ = machine_errno();
		if (machine_errno_retryable(errno_)) {
			rc = mm_io_wait(client, UINT32_MAX);
			if (rc == -1) {
				break;
			}
			continue;
		}
		break;
	}
	test(total == COMPRESSION_PAYLOAD_SIZE);

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

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(COMPRESSION_PORT);
	int rc;
	rc = mm_io_connect(client, (struct sockaddr *)&sa, UINT32_MAX);
	test(rc == 0);

	if (compression_algorithm != MM_ZPQ_NO_COMPRESSION) {
		rc = mm_io_set_compression(client, compression_algorithm);
		test(rc == 0);
	}

	machine_msg_t *msg;
	msg = machine_msg_create(0);
	test(msg != NULL);
	rc = machine_msg_write(msg, "ping", sizeof("ping"));
	test(rc == 0);
	rc = machine_write(client, msg, UINT32_MAX);
	test(rc == 0);

	msg = machine_msg_create(0);
	test(msg != NULL);
	rc = machine_msg_write(msg, compression_payload,
			       COMPRESSION_PAYLOAD_SIZE);
	test(rc == 0);
	rc = machine_write(client, msg, UINT32_MAX);
	test(rc == 0);

	msg = machine_read(client, COMPRESSION_PAYLOAD_SIZE, UINT32_MAX);
	test(msg != NULL);
	test(memcmp(machine_msg_data(msg), compression_reply,
		    COMPRESSION_PAYLOAD_SIZE) == 0);
	machine_msg_free(msg);

	msg = machine_read(client, 1, UINT32_MAX);
	/* eof */
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

static void compression_roundtrip(void)
{
	int id;
	id = machine_create("test", test_cs, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);
}

void machinarium_test_compression(void)
{
	machinarium_init();

	size_t i;
	for (i = 0; i < COMPRESSION_PAYLOAD_SIZE; i++) {
		compression_payload[i] = (char)(i % 251 + 1);
		compression_reply[i] = (char)((i * 7 + 13) % 251 + 1);
	}

	/* pick the algorithms machinarium supports; in a build without
	 * compression support this returns MM_ZPQ_NO_COMPRESSION and the
	 * test still runs as a plain roundtrip. "fz" picks zstd when
	 * available, "z" picks zlib - both backends get covered when
	 * both are compiled in */
	compression_algorithm = machine_compression_choose_alg("fz");
	compression_roundtrip();

	compression_algorithm = machine_compression_choose_alg("z");
	compression_roundtrip();

	machinarium_free();
}

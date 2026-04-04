
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <tests/odyssey_test.h>

#include <string.h>
#include <arpa/inet.h>

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

	mm_io_t *client = NULL;
	rc = mm_io_accept(server, &client, 16, 1, UINT32_MAX);
	test(rc == 0);
	test(client != NULL);

	machine_tls_t *tls;
	tls = machine_tls_create();
	rc = machine_tls_set_verify(tls, "none");
	test(rc == 0);
	rc = machine_tls_set_ca_file(tls, "./machinarium/ca.crt");
	test(rc == 0);
	rc = machine_tls_set_cert_file(tls, "./machinarium/server.crt");
	test(rc == 0);
	rc = machine_tls_set_key_file(tls, "./machinarium/server.key");
	test(rc == 0);
	rc = mm_io_set_tls(client, tls, UINT32_MAX);
	if (rc == -1) {
		printf("%s\n", mm_io_error(client));
		test(rc == 0);
	}

	machine_msg_t *msg;
	msg = machine_msg_create(0);
	test(msg != NULL);
	char text[] = "hello world";
	rc = machine_msg_write(msg, text, sizeof(text));
	test(rc == 0);

	rc = machine_write(client, msg, UINT32_MAX);
	test(rc == 0);

	rc = mm_io_close(client);
	test(rc == 0);
	mm_io_free(client);

	rc = mm_io_close(server);
	test(rc == 0);
	mm_io_free(server);

	machine_tls_free(tls);
}

static void client(void *arg)
{
	(void)arg;
	mm_io_t *client = mm_io_create();
	test(client != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(7778);
	int rc;
	rc = mm_io_connect(client, (struct sockaddr *)&sa, UINT32_MAX);
	test(rc == 0);

	machine_tls_t *tls;
	tls = machine_tls_create();
	rc = machine_tls_set_verify(tls, "none");
	test(rc == 0);
	rc = machine_tls_set_ca_file(tls, "./machinarium/ca.crt");
	test(rc == 0);
	rc = machine_tls_set_cert_file(tls, "./machinarium/client.crt");
	test(rc == 0);
	rc = machine_tls_set_key_file(tls, "./machinarium/client.key");
	test(rc == 0);
	rc = mm_io_set_tls(client, tls, UINT32_MAX);
	if (rc == -1) {
		printf("%s\n", mm_io_error(client));
		test(rc == 0);
	}

	machine_msg_t *msg;
	msg = machine_read(client, 12, UINT32_MAX);
	test(msg != NULL);
	test(memcmp(machine_msg_data(msg), "hello world", 12) == 0);
	machine_msg_free(msg);

	msg = machine_read(client, 1, UINT32_MAX);
	/* eof */
	test(msg == NULL);

	rc = mm_io_close(client);
	test(rc == 0);
	mm_io_free(client);

	machine_tls_free(tls);
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

void machinarium_test_tls0(void)
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

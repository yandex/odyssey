
#include <machinarium/machinarium.h>
#include <machinarium/eventfd.h>
#include <tests/odyssey_test.h>

#include <arpa/inet.h>

static void writer(void *a)
{
	mm_eventfd_t *efd = a;

	/* TODO: maybe some better way to wait reader to be in awaiting */
	machine_sleep(1000);

	int rc = mm_eventfd_write(efd, 30 * 1000);
	test(rc == 0);
}

static void reader(void *a)
{
	mm_eventfd_t *efd = a;

	test(mm_eventfd_attach(efd) == 0);

	int rc = mm_eventfd_read(efd, 30 * 1000);
	test(rc == 0);

	test(mm_eventfd_detach(efd) == 0);
}

static void reader_timeout(void *a)
{
	mm_eventfd_t *efd = a;

	test(mm_eventfd_attach(efd) == 0);

	int rc = mm_eventfd_read(efd, 1000);
	test(rc == -1);
	test(mm_errno_get() == ETIMEDOUT);

	test(mm_eventfd_detach(efd) == 0);
}

static void test_readwrite(void *a)
{
	(void)a;

	mm_eventfd_t efd;
	test(mm_eventfd_init(&efd) == 0);

	int rid = machine_create("reader_timeout", reader_timeout, &efd);
	test(rid != -1);

	int rc = machine_wait(rid);
	test(rc != -1);

	rid = machine_create("reader", reader, &efd);
	test(rid != -1);

	int wid = machine_create("writer", writer, &efd);
	test(wid != -1);

	rc = machine_wait(wid);
	test(rc != -1);

	rc = machine_wait(rid);
	test(rc != -1);

	mm_eventfd_destroy(&efd);
}

static void test_peering(void *a)
{
	(void)a;

	mm_eventfd_t efd;
	test(mm_eventfd_init(&efd) == 0);

	mm_eventfd_attach(&efd);

	mm_io_t *server = mm_io_create();
	test(server != NULL);

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = htons(57778);
	int rc;
	rc = mm_io_bind(server, (struct sockaddr *)&sa,
			MM_BINDWITH_SO_REUSEADDR);
	test(rc == 0);

	mm_eventfd_peer_to(&efd, server);

	int wid = machine_create("writer", writer, &efd);
	test(wid != -1);

	mm_io_t *client;
	rc = mm_io_accept(server, &client, 16, 1, 1000);
	test(rc == -1);
	test(mm_errno_get() == ETIMEDOUT);

	wid = machine_create("writer", writer, &efd);
	test(wid != -1);

	rc = mm_io_accept(server, &client, 16, 1, 30 * 1000);
	test(rc == MM_COND_WAIT_OK_PROPAGATED);

	mm_eventfd_remove_peer_to(&efd, server);

	rc = mm_io_close(server);
	test(rc == 0);

	mm_io_free(server);

	rc = machine_wait(wid);
	test(rc != -1);

	mm_eventfd_destroy(&efd);
}

void machinarium_test_eventfd(void)
{
	machinarium_init();

	int id;
	id = machine_create("readwrite", test_readwrite, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	id = machine_create("peering", test_peering, NULL);
	test(id != -1);

	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}

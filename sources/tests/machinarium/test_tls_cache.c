
/*
 * machinarium.
 *
 * TLS context cache tests: contexts are keyed by TLS options content,
 * so per-attempt tls objects with identical options share one context,
 * and machine_tls_cache_invalidate() rebuilds them.
 */

#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/tls.h>
#include <tests/odyssey_test.h>

static void test_cache(void *arg)
{
	(void)arg;

	/* two objects with identical options, like the per-attempt
	 * backend tls objects of one storage */
	machine_tls_t *tls1 = machine_tls_create();
	test(tls1 != NULL);
	test(machine_tls_set_verify(tls1, "none") == 0);

	machine_tls_t *tls2 = machine_tls_create();
	test(tls2 != NULL);
	test(machine_tls_set_verify(tls2, "none") == 0);

	mm_io_t *io1 = mm_io_create();
	mm_io_t *io2 = mm_io_create();
	test(io1 != NULL);
	test(io2 != NULL);
	io1->tls = (mm_tls_t *)tls1;
	io2->tls = (mm_tls_t *)tls2;

	SSL_CTX *ctx1 = mm_tls_get_context(io1, 1);
	test(ctx1 != NULL);

	/*
	 * Hold a reference to ctx1: an SSL keeps the context memory
	 * alive (refcounted by SSL_new), so a freshly created context
	 * can never land on the same address.
	 */
	SSL *keepalive = SSL_new(ctx1);
	test(keepalive != NULL);

	SSL_CTX *ctx2 = mm_tls_get_context(io2, 1);
	test(ctx2 != NULL);

	/* same options content -> same cached context */
	test(ctx1 == ctx2);

	/* server side uses a separate context */
	SSL_CTX *ctx3 = mm_tls_get_context(io1, 0);
	test(ctx3 != NULL);
	test(ctx3 != ctx1);

	/* different options -> different context */
	machine_tls_t *tls4 = machine_tls_create();
	test(tls4 != NULL);
	test(machine_tls_set_verify(tls4, "none") == 0);
	test(machine_tls_set_ca_file(tls4, "./machinarium/ca.crt") == 0);

	mm_io_t *io4 = mm_io_create();
	test(io4 != NULL);
	io4->tls = (mm_tls_t *)tls4;

	SSL_CTX *ctx4 = mm_tls_get_context(io4, 1);
	test(ctx4 != NULL);
	test(ctx4 != ctx1);

	/* invalidate: contexts are rebuilt in a new epoch */
	machine_tls_cache_invalidate();

	SSL_CTX *ctx5 = mm_tls_get_context(io1, 1);
	test(ctx5 != NULL);
	test(ctx5 != ctx1);

	SSL_CTX *ctx6 = mm_tls_get_context(io2, 1);
	test(ctx6 != NULL);
	test(ctx6 == ctx5);

	SSL_free(keepalive);

	mm_io_free(io1);
	mm_io_free(io2);
	mm_io_free(io4);

	machine_tls_free(tls1);
	machine_tls_free(tls2);
	machine_tls_free(tls4);
}

void machinarium_test_tls_cache(void)
{
	machinarium_init();

	int id;
	id = machine_create("test", test_cache, NULL);
	test(id != -1);

	int rc;
	rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}

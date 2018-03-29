
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

static pthread_mutex_t *mm_tls_locks;

static void
mm_tls_thread_id(CRYPTO_THREADID *tid)
{
	CRYPTO_THREADID_set_numeric(tid, (unsigned long)pthread_self());
}

static void
mm_tls_locking_callback(int mode, int type, const char *file, int line)
{
	(void)file;
	(void)line;
	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&mm_tls_locks[type]);
	else
		pthread_mutex_unlock(&mm_tls_locks[type]);
}

void mm_tls_init(void)
{
	SSL_library_init();
	SSL_load_error_strings();
	int size = CRYPTO_num_locks() * sizeof(pthread_mutex_t);
	mm_tls_locks = OPENSSL_malloc(size);
	if (mm_tls_locks == NULL) {
		abort();
		return;
	}
	int i = 0;
	for (; i < CRYPTO_num_locks(); i++)
		pthread_mutex_init(&mm_tls_locks[i], NULL);
	CRYPTO_THREADID_set_callback(mm_tls_thread_id);
	CRYPTO_set_locking_callback(mm_tls_locking_callback);
}

void mm_tls_free(void)
{
	CRYPTO_set_locking_callback(NULL);
	int i = 0;
	for (; i < CRYPTO_num_locks(); i++)
		pthread_mutex_destroy(&mm_tls_locks[i]);
	OPENSSL_free(mm_tls_locks);
#if 0
	SSL_COMP_free_compression_methods();
#endif
	FIPS_mode_set(0);
	ENGINE_cleanup();
	CONF_modules_unload(1);
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state(getpid());
	ERR_free_strings();
}

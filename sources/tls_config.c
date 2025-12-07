/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <tls_config.h>
#include <od_memory.h>

od_tls_opts_t *od_tls_opts_alloc(void)
{
	od_tls_opts_t *opts = od_malloc(sizeof(od_tls_opts_t));
	if (opts == NULL) {
		return NULL;
	}

	memset(opts, 0, sizeof(od_tls_opts_t));
	return opts;
}

od_retcode_t od_tls_opts_free(od_tls_opts_t *opts)
{
	if (opts->tls) {
		od_free(opts->tls);
	}

	if (opts->tls_ca_file) {
		od_free(opts->tls_ca_file);
	}

	if (opts->tls_key_file) {
		od_free(opts->tls_key_file);
	}

	if (opts->tls_cert_file) {
		od_free(opts->tls_cert_file);
	}

	if (opts->tls_protocols) {
		od_free(opts->tls_protocols);
	}

	od_free(opts);
	return OK_RESPONSE;
}

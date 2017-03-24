
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

MACHINE_API machine_tls_t
machine_create_tls(machine_t machine)
{
	mm_tls_t *tls;
	tls = malloc(sizeof(*tls));
	if (tls == NULL)
		return NULL;
	tls->mode = MM_TLS_PREFER;
	(void)machine;
	return tls;
}

MACHINE_API void
machine_free_tls(machine_tls_t obj)
{
	mm_tls_t *tls = obj;
	free(tls);
}

MACHINE_API int
machine_tls_set_mode(machine_tls_t obj, char *mode)
{
	mm_tls_t *tls = obj;
	if (strcasecmp(mode, "disable") == 0)
		tls->mode = MM_TLS_DISABLE;
	else
	if (strcasecmp(mode, "allow") == 0)
		tls->mode = MM_TLS_ALLOW;
	else
	if (strcasecmp(mode, "prefer") == 0)
		tls->mode = MM_TLS_ALLOW;
	else
	if (strcasecmp(mode, "require") == 0)
		tls->mode = MM_TLS_REQUIRE;
	else
	if (strcasecmp(mode, "verify_ca") == 0)
		tls->mode = MM_TLS_VERIFY_CA;
	else
	if (strcasecmp(mode, "verify_full") == 0)
		tls->mode = MM_TLS_VERIFY_FULL;
	else
		return -1;
	return 0;
}

MACHINE_API int
machine_tls_set_protocols(machine_tls_t obj, char *protocols)
{
	mm_tls_t *tls = obj;
	(void)tls;
	(void)protocols;
	return 0;
}

MACHINE_API int
machine_tls_set_ca_path(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	(void)tls;
	(void)path;
	return 0;
}

MACHINE_API int
machine_tls_set_ca_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	(void)tls;
	(void)path;
	return 0;
}

MACHINE_API int
machine_tls_set_ca(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	(void)tls;
	(void)data;
	(void)size;
	return 0;
}

MACHINE_API int
machine_tls_set_cert_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	(void)tls;
	(void)path;
	return 0;
}

MACHINE_API int
machine_tls_set_cert(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	(void)tls;
	(void)data;
	(void)size;
	return 0;
}

MACHINE_API int
machine_tls_set_key_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	(void)tls;
	(void)path;
	return 0;
}

MACHINE_API int
machine_tls_set_key(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	(void)tls;
	(void)data;
	(void)size;
	return 0;
}


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
	(void)machine;
	mm_tls_t *tls;
	tls = malloc(sizeof(*tls));
	if (tls == NULL)
		return NULL;
	tls->config = tls_config_new();
	if (tls->config == NULL) {
		free(tls);
		return NULL;
	}
	return tls;
}

MACHINE_API void
machine_free_tls(machine_tls_t obj)
{
	mm_tls_t *tls = obj;
	tls_config_free(tls->config);
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
	uint32_t id = 0;
	int rc;
	rc = tls_config_parse_protocols(&id, protocols);
	if (rc < 0)
		return -1;
	tls_config_set_protocols(tls->config, id);
	return 0;
}

MACHINE_API int
machine_tls_set_ca_path(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	return tls_config_set_ca_path(tls->config, path);
}

MACHINE_API int
machine_tls_set_ca_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	return tls_config_set_ca_file(tls->config, path);
}

MACHINE_API int
machine_tls_set_ca(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	return tls_config_set_ca_mem(tls->config, (uint8_t*)data, size);
}

MACHINE_API int
machine_tls_set_cert_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	return tls_config_set_cert_file(tls->config, path);
}

MACHINE_API int
machine_tls_set_cert(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	return tls_config_set_cert_mem(tls->config, (uint8_t*)data, size);
}

MACHINE_API int
machine_tls_set_key_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	return tls_config_set_key_file(tls->config, path);
}

MACHINE_API int
machine_tls_set_key(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	return tls_config_set_key_mem(tls->config, (uint8_t*)data, size);
}

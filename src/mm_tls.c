
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
	tls->mode      = MM_TLS_PREFER;
	tls->protocols = NULL;
	tls->ca_path   = NULL;
	tls->ca_file   = NULL;
	tls->ca        = NULL;
	tls->ca_size   = 0;
	tls->cert_file = NULL;
	tls->cert      = NULL;
	tls->cert_size = 0;
	tls->key_file  = NULL;
	tls->key       = NULL;
	tls->key_size  = 0;
	(void)machine;
	return tls;
}

MACHINE_API void
machine_free_tls(machine_tls_t obj)
{
	mm_tls_t *tls = obj;
	if (tls->protocols)
		free(tls->protocols);
	if (tls->ca_path)
		free(tls->ca_path);
	if (tls->ca_file)
		free(tls->ca_file);
	if (tls->ca)
		free(tls->ca);
	if (tls->cert_file)
		free(tls->cert_file);
	if (tls->cert)
		free(tls->cert);
	if (tls->key_file)
		free(tls->key_file);
	if (tls->key)
		free(tls->key);
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
	char *string = strdup(protocols);
	if (string == NULL)
		return -1;
	if (tls->protocols)
		free(tls->protocols);
	tls->protocols = string;
	return 0;
}

MACHINE_API int
machine_tls_set_ca_path(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	char *string = strdup(path);
	if (string == NULL)
		return -1;
	if (tls->ca_path)
		free(tls->ca_path);
	tls->ca_path = string;
	return 0;
}

MACHINE_API int
machine_tls_set_ca_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	char *string = strdup(path);
	if (string == NULL)
		return -1;
	if (tls->ca_file)
		free(tls->ca_file);
	tls->ca_file = string;
	return 0;
}

MACHINE_API int
machine_tls_set_ca(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	char *key = malloc(size);
	if (key == NULL)
		return -1;
	memcpy(key, data, size);
	if (tls->ca)
		free(tls->ca);
	tls->ca = key;
	tls->ca_size = size;
	return 0;
}

MACHINE_API int
machine_tls_set_cert_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	char *string = strdup(path);
	if (string == NULL)
		return -1;
	if (tls->cert_file)
		free(tls->cert_file);
	tls->cert_file = string;
	return 0;
}

MACHINE_API int
machine_tls_set_cert(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	char *key = malloc(size);
	if (key == NULL)
		return -1;
	memcpy(key, data, size);
	if (tls->cert)
		free(tls->cert);
	tls->cert = key;
	tls->cert_size = size;
	return 0;
}

MACHINE_API int
machine_tls_set_key_file(machine_tls_t obj, char *path)
{
	mm_tls_t *tls = obj;
	char *string = strdup(path);
	if (string == NULL)
		return -1;
	if (tls->key_file)
		free(tls->key_file);
	tls->key_file = string;
	return 0;
}

MACHINE_API int
machine_tls_set_key(machine_tls_t obj, char *data, int size)
{
	mm_tls_t *tls = obj;
	char *key = malloc(size);
	if (key == NULL)
		return -1;
	memcpy(key, data, size);
	if (tls->key)
		free(tls->key);
	tls->key = key;
	tls->key_size = size;
	return 0;
}

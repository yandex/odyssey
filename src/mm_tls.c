
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
	tls->verify    = MM_TLS_NONE;
	tls->protocols = NULL;
	tls->ca_path   = NULL;
	tls->ca_file   = NULL;
	tls->cert_file = NULL;
	tls->key_file  = NULL;
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
	if (tls->cert_file)
		free(tls->cert_file);
	if (tls->key_file)
		free(tls->key_file);
	free(tls);
}

MACHINE_API int
machine_tls_set_verify(machine_tls_t obj, char *mode)
{
	mm_tls_t *tls = obj;
	if (strcasecmp(mode, "none") == 0)
		tls->verify = MM_TLS_NONE;
	else
	if (strcasecmp(mode, "peer") == 0)
		tls->verify = MM_TLS_PEER;
	else
	if (strcasecmp(mode, "peer_strict") == 0)
		tls->verify = MM_TLS_PEER_STRICT;
	else
		return -1;
	return 0;
}

MACHINE_API int
machine_tls_set_server(machine_tls_t obj, char *name)
{
	mm_tls_t *tls = obj;
	char *string = strdup(name);
	if (string == NULL)
		return -1;
	if (tls->server)
		free(tls->server);
	tls->server = string;
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

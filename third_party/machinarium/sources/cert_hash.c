
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API ssize_t machine_tls_cert_hash(
	machine_io_t *obj, unsigned char (*cert_hash)[MM_CERT_HASH_LEN],
	unsigned int *len)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return mm_tls_get_cert_hash(io, cert_hash, len);
}
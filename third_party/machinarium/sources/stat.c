
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API int machine_io_sysfd(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return io->fd;
}

MACHINE_API int machine_io_mask(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return io->handle.mask;
}

MACHINE_API int machine_io_mm_fd(machine_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return io->handle.fd;
}

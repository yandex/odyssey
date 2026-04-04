
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium/machinarium.h>
#include <machinarium/io.h>

MACHINE_API int machine_io_sysfd(mm_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return io->fd;
}

MACHINE_API int machine_io_mask(mm_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return io->handle.mask;
}

MACHINE_API int machine_io_mm_fd(mm_io_t *obj)
{
	mm_io_t *io = mm_cast(mm_io_t *, obj);
	return io->handle.fd;
}

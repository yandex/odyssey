
#include <machinarium/machinarium.h>
#include <machinarium/io.h>
#include <machinarium/machine.h>

int mm_io_shutdown(mm_io_t *io)
{
	mm_errno_set(0);

	int rc = shutdown(io->fd, SHUT_RDWR);

	if (rc == -1) {
		return MM_NOTOK_RETCODE;
	}

	return MM_OK_RETCODE;
}

int mm_io_shutdown_receptions(mm_io_t *io)
{
	mm_errno_set(0);

	int rc = shutdown(io->fd, SHUT_RD);

	if (rc == -1) {
		return MM_NOTOK_RETCODE;
	}

	return MM_OK_RETCODE;
}

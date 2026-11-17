/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/build.h>
#include <machinarium/uring.h>

#ifdef HAVE_IO_URING
#include <liburing.h>
#include <stdlib.h>

static int mm_uring_probe_error(int error)
{
	errno = error == ENOSYS || error == EINVAL || error == EOPNOTSUPP ?
			ENOTSUP :
			error;
	return -1;
}

int mm_uring_probe(void)
{
	struct io_uring ring;
	struct io_uring_params params = { 0 };
	int rc = io_uring_queue_init_params(8, &ring, &params);
	if (rc < 0) {
		return mm_uring_probe_error(-rc);
	}

	int error = ENOTSUP;
	struct io_uring_probe *probe = NULL;
	unsigned features = IORING_FEAT_EXT_ARG | IORING_FEAT_NODROP;
	if ((params.features & features) != features) {
		goto done;
	}

	/* Register explicitly: liburing 2.2's get_probe helper loses errno. */
	probe = calloc(1, sizeof(*probe) + 256 * sizeof(probe->ops[0]));
	if (probe == NULL) {
		error = ENOMEM;
		goto done;
	}
	rc = io_uring_register_probe(&ring, probe, 256);
	if (rc < 0) {
		error = -rc;
		goto done;
	}

	const int operations[] = { IORING_OP_POLL_ADD, IORING_OP_ASYNC_CANCEL,
				   IORING_OP_RECV, IORING_OP_SENDMSG };
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]);
	     i++) {
		if (!io_uring_opcode_supported(probe, operations[i])) {
			goto done;
		}
	}
	error = 0;
done:
	free(probe);
	io_uring_queue_exit(&ring);
	if (error) {
		return mm_uring_probe_error(error);
	}
	return 0;
}
#else
int mm_uring_probe(void)
{
	errno = ENOTSUP;
	return -1;
}
#endif

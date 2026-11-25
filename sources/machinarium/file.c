
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 *
 * Portable layer of the mm_file* API: async execution on the
 * machinarium file pool and the plain/async dispatch. The plain
 * primitives live in a per-platform file (file_posix.c today),
 * selected by the build in the same way as the epoll/kqueue poll
 * backends.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <machinarium/file.h>
#include <machinarium/machinarium.h>
#include <machinarium/memory.h>
#include <machinarium/mm.h>
#include <machinarium/thread_pool.h>

/*
 * Asynchronous file I/O.
 *
 * Blocking file operations are executed on a machinarium thread pool
 * (machinarium.file_pool, started by machinarium_init()). The calling
 * coroutine parks until the result is ready, so other machines keep
 * running.
 *
 * When the pool is not running (before machinarium_init(), during
 * shutdown or when the pool is disabled by configuration), the calls
 * fall back to the plain synchronous implementations: blocking is
 * legal in those contexts and the startup code paths keep working.
 *
 * Errors (errno):
 *  - EAGAIN: the pool queue is full, the task was not submitted;
 *  - ENOMEM: out of memory;
 *  - ECANCELED: the calling machine was cancelled while waiting, the
 *    task is skipped by the worker and is not executed;
 *  - otherwise: the errno of the underlying file operation.
 */

enum {
	MM_FILE_OP_READ_ALL = 0,
	MM_FILE_OP_OPEN,
	MM_FILE_OP_OPEN_DIR,
	MM_FILE_OP_WRITE_ALL,
	MM_FILE_OP_FSYNC,
	MM_FILE_OP_UNLINK,
	MM_FILE_OP_RENAME,
	MM_FILE_OP_CHMOD,
	MM_FILE_OP_FLOCK
};

/* the task argument, owned by the pool queue / worker */
typedef struct {
	int op;
	int fd;
	mode_t mode;
	size_t size;
	const void *buf;
	char path[1];
} mm_file_arg_t;

/* the task result, transferred to the caller via the future */
typedef struct {
	int rc;
	int errno_;
	int fd;
	char *buf;
	size_t size;
} mm_file_result_t;

static void mm_file_result_free(void *obj)
{
	mm_file_result_t *res = obj;
	/* buf is transferred to the caller on success, so here it is
	 * only set on failure */
	if (res->buf != NULL) {
		mm_free(res->buf);
	}
	mm_free(res);
}

static void *mm_file_task(void *obj)
{
	mm_file_arg_t *arg = obj;

	mm_file_result_t *res = mm_malloc(sizeof(*res));
	if (res == NULL) {
		return NULL;
	}
	memset(res, 0, sizeof(*res));

	errno = 0;
	switch (arg->op) {
	case MM_FILE_OP_READ_ALL:
		res->rc = mm_file_read_all_plain(arg->path, &res->buf,
						 &res->size);
		break;
	case MM_FILE_OP_OPEN:
		res->fd = mm_file_open_plain(arg->path, arg->fd, arg->mode);
		res->rc = res->fd == -1 ? -1 : 0;
		break;
	case MM_FILE_OP_OPEN_DIR:
		res->fd = mm_file_open_dir_plain(arg->path);
		res->rc = res->fd == -1 ? -1 : 0;
		break;
	case MM_FILE_OP_WRITE_ALL:
		res->rc = mm_file_write_all_plain(arg->fd, arg->buf, arg->size);
		break;
	case MM_FILE_OP_FSYNC:
		res->rc = mm_file_fsync_plain(arg->fd);
		break;
	case MM_FILE_OP_UNLINK:
		res->rc = mm_file_unlink_plain(arg->path);
		break;
	case MM_FILE_OP_RENAME:
		res->rc = mm_file_rename_plain(arg->path, arg->buf);
		break;
	case MM_FILE_OP_CHMOD:
		res->rc = mm_file_chmod_plain(arg->path, arg->mode);
		break;
	case MM_FILE_OP_FLOCK:
		res->rc = mm_file_flock_plain(arg->fd, arg->mode);
		break;
	default:
		abort();
	}
	res->errno_ = errno;
	return res;
}

/*
 * Runs a file task on the pool. The buffer for write operations is
 * expected to be at arg->path (write_all writes 'size' bytes from
 * there), so the data and the path share the same allocation.
 *
 * Takes ownership of 'arg' in all cases.
 * On success returns a heap result, the caller must free it with
 * mm_file_result_free(). Returns NULL with errno set on submit/wait
 * errors.
 */
static mm_file_result_t *mm_file_run(mm_file_arg_t *arg, const void *buf)
{
	arg->buf = buf;
	mm_thread_pool_t *pool = machinarium.file_pool;

	/* no pool (startup/shutdown): run synchronously */
	if (pool == NULL) {
		mm_file_result_t res;
		memset(&res, 0, sizeof(res));
		errno = 0;
		switch (arg->op) {
		case MM_FILE_OP_READ_ALL:
			res.rc = mm_file_read_all_plain(arg->path, &res.buf,
							&res.size);
			break;
		case MM_FILE_OP_OPEN:
			res.fd = mm_file_open_plain(arg->path, arg->fd,
						    arg->mode);
			res.rc = res.fd == -1 ? -1 : 0;
			break;
		case MM_FILE_OP_OPEN_DIR:
			res.fd = mm_file_open_dir_plain(arg->path);
			res.rc = res.fd == -1 ? -1 : 0;
			break;
		case MM_FILE_OP_WRITE_ALL:
			res.rc = mm_file_write_all_plain(arg->fd, arg->buf,
							 arg->size);
			break;
		case MM_FILE_OP_FSYNC:
			res.rc = mm_file_fsync_plain(arg->fd);
			break;
		case MM_FILE_OP_UNLINK:
			res.rc = mm_file_unlink_plain(arg->path);
			break;
		case MM_FILE_OP_RENAME:
			res.rc = mm_file_rename_plain(arg->path, arg->buf);
			break;
		case MM_FILE_OP_CHMOD:
			res.rc = mm_file_chmod_plain(arg->path, arg->mode);
			break;
		case MM_FILE_OP_FLOCK:
			res.rc = mm_file_flock_plain(arg->fd, arg->mode);
			break;
		default:
			abort();
		}
		res.errno_ = errno;

		mm_file_result_t *copy = mm_malloc(sizeof(*copy));
		if (copy == NULL) {
			errno = ENOMEM;
			mm_free(arg);
			return NULL;
		}
		memcpy(copy, &res, sizeof(*copy));
		mm_free(arg);
		return copy;
	}

	mm_future_t *future = mm_thread_pool_submit(
		pool, mm_file_task, arg, mm_free, mm_file_result_free, 0);
	if (future == NULL) {
		/* errno is set by submit (EAGAIN or ENOMEM) */
		mm_free(arg);
		return NULL;
	}

	int rc = mm_thread_pool_wait(future, UINT32_MAX);
	if (rc == -1) {
		/*
		 * The machine was cancelled: the worker will skip the
		 * task (it is still owned by the queue) and the caller
		 * sees ECANCELED.
		 */
		errno = ECANCELED;
		mm_future_unref(future);
		return NULL;
	}

	mm_file_result_t *res = mm_future_get_result(future);
	if (res == NULL) {
		/* out of memory in the worker */
		errno = ENOMEM;
		mm_future_unref(future);
		return NULL;
	}

	/* the result is owned by the future, make a copy for the caller */
	mm_file_result_t *copy = mm_malloc(sizeof(*copy));
	if (copy == NULL) {
		errno = ENOMEM;
		mm_future_unref(future);
		return NULL;
	}
	memcpy(copy, res, sizeof(*copy));

	/* transfer the buffer to the caller */
	res->buf = NULL;

	mm_future_unref(future);
	return copy;
}

int mm_file_read_all(const char *path, char **buf, size_t *size)
{
	*buf = NULL;
	*size = 0;

	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_read_all_plain(path, buf, size);
	}

	size_t path_len = strlen(path);
	mm_file_arg_t *arg = mm_malloc(sizeof(mm_file_arg_t) + path_len);
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_READ_ALL;
	memcpy(arg->path, path, path_len + 1);

	mm_file_result_t *res = mm_file_run(arg, NULL);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	} else {
		*buf = res->buf;
		*size = res->size;
		res->buf = NULL; /* ownership transferred to the caller */
	}
	mm_file_result_free(res);
	return rc;
}

int mm_file_open(const char *path, int flags, int mode)
{
	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_open_plain(path, flags, mode);
	}

	size_t path_len = strlen(path);
	mm_file_arg_t *arg = mm_malloc(sizeof(mm_file_arg_t) + path_len);
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_OPEN;
	arg->fd = flags;
	arg->mode = mode;
	memcpy(arg->path, path, path_len + 1);

	mm_file_result_t *res = mm_file_run(arg, NULL);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	}
	int fd = res->fd;
	mm_file_result_free(res);
	return rc == 0 ? fd : -1;
}

int mm_file_open_dir(const char *path)
{
	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_open_dir_plain(path);
	}

	size_t path_len = strlen(path);
	mm_file_arg_t *arg = mm_malloc(sizeof(mm_file_arg_t) + path_len);
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_OPEN_DIR;
	memcpy(arg->path, path, path_len + 1);

	mm_file_result_t *res = mm_file_run(arg, NULL);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	}
	int fd = res->fd;
	mm_file_result_free(res);
	return rc == 0 ? fd : -1;
}

int mm_file_write_all(int fd, const void *buf, size_t size)
{
	/*
	 * The caller owns 'buf' and 'fd' and is blocked until the task
	 * completes, so both are guaranteed to be alive while the worker
	 * uses them.
	 */
	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_write_all_plain(fd, buf, size);
	}

	mm_file_arg_t *arg = mm_malloc(sizeof(mm_file_arg_t));
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_WRITE_ALL;
	arg->fd = fd;
	arg->size = size;

	mm_file_result_t *res = mm_file_run(arg, buf);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	}
	mm_file_result_free(res);
	return rc;
}

int mm_file_fsync(int fd)
{
	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_fsync_plain(fd);
	}

	mm_file_arg_t *arg = mm_malloc(sizeof(mm_file_arg_t));
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_FSYNC;
	arg->fd = fd;

	mm_file_result_t *res = mm_file_run(arg, NULL);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	}
	mm_file_result_free(res);
	return rc;
}

int mm_file_unlink(const char *path)
{
	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_unlink_plain(path);
	}

	size_t path_len = strlen(path);
	mm_file_arg_t *arg = mm_malloc(sizeof(mm_file_arg_t) + path_len);
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_UNLINK;
	memcpy(arg->path, path, path_len + 1);

	mm_file_result_t *res = mm_file_run(arg, NULL);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	}
	mm_file_result_free(res);
	return rc;
}

int mm_file_rename(const char *oldpath, const char *newpath)
{
	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_rename_plain(oldpath, newpath);
	}

	size_t old_len = strlen(oldpath);
	size_t new_len = strlen(newpath);
	mm_file_arg_t *arg =
		mm_malloc(sizeof(mm_file_arg_t) + old_len + 1 + new_len + 1);
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_RENAME;
	memcpy(arg->path, oldpath, old_len + 1);
	memcpy(arg->path + old_len + 1, newpath, new_len + 1);

	mm_file_result_t *res = mm_file_run(arg, arg->path + old_len + 1);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	}
	mm_file_result_free(res);
	return rc;
}

int mm_file_chmod(const char *path, mode_t mode)
{
	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_chmod_plain(path, mode);
	}

	size_t path_len = strlen(path);
	mm_file_arg_t *arg = mm_malloc(sizeof(mm_file_arg_t) + path_len);
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_CHMOD;
	arg->mode = mode;
	memcpy(arg->path, path, path_len + 1);

	mm_file_result_t *res = mm_file_run(arg, NULL);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	}
	mm_file_result_free(res);
	return rc;
}

int mm_file_flock(int fd, int operation)
{
	mm_thread_pool_t *pool = machinarium.file_pool;
	if (pool == NULL) {
		return mm_file_flock_plain(fd, operation);
	}

	mm_file_arg_t *arg = mm_malloc(sizeof(mm_file_arg_t));
	if (arg == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(arg, 0, sizeof(*arg));
	arg->op = MM_FILE_OP_FLOCK;
	arg->fd = fd;
	arg->mode = operation;

	mm_file_result_t *res = mm_file_run(arg, NULL);
	if (res == NULL) {
		return -1;
	}

	int rc = res->rc;
	if (rc == -1) {
		errno = res->errno_;
	}
	mm_file_result_free(res);
	return rc;
}

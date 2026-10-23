
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 *
 * POSIX backend for the mm_file* API: the plain functions below are
 * the only place where the API meets the platform. A port to another
 * system means providing the same set of functions on top of its
 * native file API.
 */

#include <errno.h>
#include <stdio.h>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <machinarium/file.h>
#include <machinarium/memory.h>

/*
 * Maps the MM_OPEN_* flags to the open(2) O_* flags. This is the
 * only place where the machinarium flags meet the platform ones.
 */
static int mm_open_flags_posix(int flags)
{
	int out = 0;
	if (flags & MM_OPEN_RDONLY) {
		out |= O_RDONLY;
	}
	if (flags & MM_OPEN_WRONLY) {
		out |= O_WRONLY;
	}
	if (flags & MM_OPEN_RDWR) {
		out |= O_RDWR;
	}
	if (flags & MM_OPEN_CREAT) {
		out |= O_CREAT;
	}
	if (flags & MM_OPEN_EXCL) {
		out |= O_EXCL;
	}
	if (flags & MM_OPEN_TRUNC) {
		out |= O_TRUNC;
	}
	if (flags & MM_OPEN_APPEND) {
		out |= O_APPEND;
	}
	if (flags & MM_OPEN_NOFOLLOW) {
		out |= O_NOFOLLOW;
	}
	if (flags & MM_OPEN_DIRECTORY) {
		out |= O_DIRECTORY;
	}
	if (flags & MM_OPEN_CLOEXEC) {
		out |= O_CLOEXEC;
	}
	return out;
}

/*
 * Maps the MM_LOCK_* flags to the flock(2) LOCK_* flags.
 */
static int mm_lock_flags_posix(int op)
{
	int out = 0;
	if (op & MM_LOCK_SH) {
		out |= LOCK_SH;
	}
	if (op & MM_LOCK_EX) {
		out |= LOCK_EX;
	}
	if (op & MM_LOCK_NB) {
		out |= LOCK_NB;
	}
	return out;
}

int mm_file_open_plain(const char *path, int flags, int mode)
{
	return open(path, mm_open_flags_posix(flags) | O_CLOEXEC, mode);
}

int mm_file_close(int fd)
{
	return close(fd);
}

ssize_t mm_file_read(int fd, void *buf, size_t size)
{
	ssize_t rc;
	do {
		rc = read(fd, buf, size);
	} while (rc < 0 && errno == EINTR);
	return rc;
}

ssize_t mm_file_write(int fd, const void *buf, size_t size)
{
	ssize_t rc;
	do {
		rc = write(fd, buf, size);
	} while (rc < 0 && errno == EINTR);
	return rc;
}

ssize_t mm_file_writev(int fd, const struct iovec *iov, int iovc)
{
	ssize_t rc;
	do {
		rc = writev(fd, iov, iovc);
	} while (rc < 0 && errno == EINTR);
	return rc;
}

int mm_file_write_all_plain(int fd, const void *buf, size_t size)
{
	size_t done = 0;
	while (done < size) {
		ssize_t rc = write(fd, (const char *)buf + done, size - done);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		done += (size_t)rc;
	}
	return 0;
}

int mm_file_read_all_plain(const char *path, char **buf, size_t *size)
{
	*buf = NULL;
	*size = 0;

	int fd = mm_file_open_plain(path, MM_OPEN_RDONLY, 0);
	if (fd < 0) {
		return -1;
	}

	/* the file size is only a hint: it is zero or stale for
	 * procfs and other pseudo-files */
	size_t cap = 4096;
	struct stat st;
	if (mm_file_fstat(fd, &st) == 0 && st.st_size > 0) {
		cap = (size_t)st.st_size + 1;
	}

	char *data = mm_malloc(cap);
	if (data == NULL) {
		mm_file_close(fd);
		errno = ENOMEM;
		return -1;
	}

	size_t used = 0;
	for (;;) {
		if (used + 1 == cap) {
			char *grown = mm_realloc(data, cap * 2);
			if (grown == NULL) {
				mm_free(data);
				mm_file_close(fd);
				errno = ENOMEM;
				return -1;
			}
			data = grown;
			cap *= 2;
		}

		ssize_t rc = mm_file_read(fd, data + used, cap - used - 1);
		if (rc == -1) {
			int errno_ = errno;
			mm_free(data);
			mm_file_close(fd);
			errno = errno_;
			return -1;
		}
		if (rc == 0) {
			break;
		}
		used += (size_t)rc;
	}

	mm_file_close(fd);

	data[used] = '\0';
	*buf = data;
	*size = used;
	return 0;
}

int mm_file_fsync_plain(int fd)
{
	return fsync(fd);
}

int mm_file_fstat(int fd, struct stat *st)
{
	return fstat(fd, st);
}

int mm_file_lstat(const char *path, struct stat *st)
{
	return lstat(path, st);
}

int mm_file_is_regular(int fd)
{
	struct stat st;
	if (fstat(fd, &st) == -1) {
		return -1;
	}
	return S_ISREG(st.st_mode) ? 1 : 0;
}

int mm_file_is_socket(const char *path, dev_t *dev, ino_t *ino)
{
	struct stat st;
	if (lstat(path, &st) == -1) {
		return -1;
	}
	if (!S_ISSOCK(st.st_mode)) {
		return 0;
	}
	if (dev) {
		*dev = st.st_dev;
	}
	if (ino) {
		*ino = st.st_ino;
	}
	return 1;
}

int mm_file_unlink_plain(const char *path)
{
	return unlink(path);
}

int mm_file_rename_plain(const char *oldpath, const char *newpath)
{
	return rename(oldpath, newpath);
}

int mm_file_chmod_plain(const char *path, mode_t mode)
{
	return chmod(path, mode);
}

int mm_file_flock_plain(int fd, int operation)
{
	return flock(fd, mm_lock_flags_posix(operation));
}

int mm_file_open_dir_plain(const char *path)
{
	return open(path,
		    mm_open_flags_posix(MM_OPEN_RDONLY | MM_OPEN_DIRECTORY) |
			    O_CLOEXEC);
}

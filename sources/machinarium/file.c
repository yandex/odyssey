
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <machinarium/file.h>
#include <machinarium/machinarium.h>
#include <machinarium/memory.h>

int mm_file_open(const char *path, int flags, int mode)
{
	return open(path, flags | O_CLOEXEC, mode);
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

int mm_file_write_all(int fd, const void *buf, size_t size)
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

int mm_file_read_all(const char *path, char **buf, size_t *size)
{
	*buf = NULL;
	*size = 0;

	int fd = mm_file_open(path, O_RDONLY, 0);
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

int mm_file_fsync(int fd)
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

int mm_file_unlink(const char *path)
{
	return unlink(path);
}

int mm_file_rename(const char *oldpath, const char *newpath)
{
	return rename(oldpath, newpath);
}

int mm_file_chmod(const char *path, mode_t mode)
{
	return chmod(path, mode);
}

int mm_file_flock(int fd, int operation)
{
	return flock(fd, operation);
}

int mm_file_open_dir(const char *path)
{
	return open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
}

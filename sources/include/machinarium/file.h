#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

/*
 * open() flags for the mm_file_open* API.
 *
 * These are machinarium's own flags and are not tied to the POSIX
 * O_* values: the implementation maps them to the platform
 * equivalents (open(2) flags on Unix), so the API and its callers
 */
#define MM_OPEN_RDONLY (1u << 0)
#define MM_OPEN_WRONLY (1u << 1)
#define MM_OPEN_RDWR (1u << 2)
#define MM_OPEN_CREAT (1u << 3)
#define MM_OPEN_EXCL (1u << 4)
#define MM_OPEN_TRUNC (1u << 5)
#define MM_OPEN_APPEND (1u << 6)
#define MM_OPEN_NOFOLLOW (1u << 7)
#define MM_OPEN_DIRECTORY (1u << 8)
#define MM_OPEN_CLOEXEC (1u << 9)

/*
 * flock() operations for mm_file_flock*(), see flock(2). Machinarium's
 * own flags mapped to the platform equivalents by the file backend,
 * the same way as MM_OPEN_*.
 */
#define MM_LOCK_SH (1u << 0)
#define MM_LOCK_EX (1u << 1)
#define MM_LOCK_NB (1u << 2)

/*
 * File creation modes for mm_file_open*() and mm_file_chmod*().
 * Semantic aliases over the POSIX S_* permission bits, the same
 * approach as PostgreSQL's pg_file_create_mode: the octal meaning is
 * universal across POSIX systems, and platforms that do not honor a
 * create mode ignore it.
 */

/* mode for creating files readable and writable by the owner only */
#define MM_FILE_MODE_OWNER (S_IRUSR | S_IWUSR)

/* mode for creating files that allows group read */
#define MM_FILE_MODE_GROUP (S_IRUSR | S_IWUSR | S_IRGRP)

/* mode for creating files that allows group and world read */
#define MM_FILE_MODE_WORLD (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/*
 * The 'plain' API below performs the operation synchronously on the
 * calling thread: the blocking syscall is done directly. Use it where
 * blocking is legal (e.g. the logger: it already serializes writes
 * through its own queue, or the startup code paths before the
 * event loop exists).
 */

int mm_file_open_plain(const char *, int, int);
int mm_file_close(int);
ssize_t mm_file_read(int, void *, size_t);
ssize_t mm_file_write(int, const void *, size_t);
ssize_t mm_file_writev(int, const struct iovec *, int);
int mm_file_write_all_plain(int, const void *, size_t);

/*
 * Reads the whole file into a NUL-terminated buffer allocated with
 * mm_malloc(); the caller must free it with mm_free(). Pseudo-files
 * with zero st_size (procfs) are supported.
 */
int mm_file_read_all_plain(const char *, char **, size_t *);

int mm_file_fsync_plain(int);
int mm_file_fstat(int, struct stat *);
int mm_file_lstat(const char *, struct stat *);

/* 1 = the fd refers to a regular file, 0 = not, -1 = error (errno set). */
int mm_file_is_regular(int);

/*
 * 1 = path is a socket (lstat, symlinks are not followed; dev/ino are
 * stored when the pointers are not NULL), 0 = the path exists but is
 * not a socket, -1 = error incl. a missing path (errno set, ENOENT
 * included).
 */
int mm_file_is_socket(const char *, dev_t *, ino_t *);
int mm_file_unlink_plain(const char *);
int mm_file_rename_plain(const char *, const char *);
int mm_file_chmod_plain(const char *, mode_t);
int mm_file_flock_plain(int, int);
int mm_file_open_dir_plain(const char *);

/*
 * The API below performs the same operations as the 'plain' one, but
 * the blocking part is executed on a machinarium file pool worker and
 * the calling coroutine is parked until the result is ready, so other
 * machines keep running. When no pool is running (e.g. before
 * machinarium_init() or during shutdown), the call falls back to the
 * plain synchronous implementation.
 *
 * Errors:
 *  - EAGAIN: the pool queue is full (task was not submitted);
 *  - ECANCELED: the calling machine was cancelled while waiting;
 *  - otherwise: the errno of the underlying file operation.
 */

/*
 * Reads the whole file into a NUL-terminated buffer allocated with
 * mm_malloc(); the caller must free it with mm_free(). Pseudo-files
 * with zero st_size (procfs) are supported.
 */
int mm_file_read_all(const char *, char **, size_t *);

int mm_file_open(const char *, int, int);
int mm_file_open_dir(const char *);
int mm_file_write_all(int, const void *, size_t);
int mm_file_fsync(int);
int mm_file_unlink(const char *);
int mm_file_rename(const char *, const char *);
int mm_file_chmod(const char *, mode_t);
int mm_file_flock(int, int);

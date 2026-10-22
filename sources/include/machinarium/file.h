#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <sys/stat.h>
#include <sys/types.h>

int mm_file_open(const char *, int, int);
int mm_file_close(int);
ssize_t mm_file_read(int, void *, size_t);
ssize_t mm_file_write(int, const void *, size_t);
int mm_file_write_all(int, const void *, size_t);

/*
 * Reads the whole file into a NUL-terminated buffer allocated with
 * mm_malloc(); the caller must free it with mm_free(). Pseudo-files
 * with zero st_size (procfs) are supported.
 */
int mm_file_read_all(const char *, char **, size_t *);

int mm_file_fsync(int);
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
int mm_file_unlink(const char *);
int mm_file_rename(const char *, const char *);
int mm_file_chmod(const char *, mode_t);
int mm_file_flock(int, int);
int mm_file_open_dir(const char *);

/*
 * Asynchronous file I/O.
 *
 * The operations below are wrappers around the synchronous API. When
 * a machinarium file pool is running, the blocking part is executed
 * on a pool worker and the calling coroutine is parked until the
 * result is ready, so other machines keep running. When no pool is
 * running (e.g. before machinarium_init() or during shutdown), the
 * call falls back to the plain synchronous implementation.
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
int mm_file_read_all_nonblock(const char *, char **, size_t *);

int mm_file_open_nonblock(const char *, int, int);
int mm_file_open_dir_nonblock(const char *);
int mm_file_write_all_nonblock(int, const void *, size_t);
int mm_file_fsync_nonblock(int);
int mm_file_unlink_nonblock(const char *);
int mm_file_rename_nonblock(const char *, const char *);
int mm_file_chmod_nonblock(const char *, mode_t);
int mm_file_flock_nonblock(int, int);

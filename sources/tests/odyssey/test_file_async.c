
/*
 * Odyssey.
 *
 * Tests for asynchronous file operations on the machinarium file pool.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <machinarium/machinarium.h>
#include <machinarium/machine.h>
#include <machinarium/file.h>
#include <machinarium/memory.h>
#include <machinarium/util.h>

#include <odyssey.h>
#include <tests/odyssey_test.h>

static char tmp_path[128];

static void tmp_path_init(void)
{
	mm_snprintf(tmp_path, sizeof(tmp_path),
		    "/tmp/odyssey_test_file_XXXXXX");
	int fd = mkstemp(tmp_path);
	test(fd != -1);
	test(close(fd) == 0);
}

static void tmp_path_free(void)
{
	unlink(tmp_path);
}

static void test_read_all(void *arg)
{
	(void)arg;

	const char *content = "hello machinarium file pool";
	size_t content_len = strlen(content);

	/* write the file synchronously, we test the read path here */
	int fd = mm_file_open_plain(tmp_path, O_WRONLY | O_TRUNC, 0644);
	test(fd != -1);
	test(mm_file_write_all_plain(fd, content, content_len) == 0);
	test(mm_file_close(fd) == 0);

	char *buf;
	size_t size;
	test(mm_file_read_all(tmp_path, &buf, &size) == 0);
	test(size == content_len);
	test(memcmp(buf, content, content_len) == 0);
	test(buf[size] == '\0');
	mm_free(buf);
}

static void test_read_all_missing(void *arg)
{
	(void)arg;

	char *buf;
	size_t size;
	test(mm_file_read_all("/tmp/odyssey_test_file_not_exists", &buf,
			      &size) == -1);
	test(errno == ENOENT);
	test(buf == NULL);
	test(size == 0);
}

static void test_open_write_read_cycle(void *arg)
{
	(void)arg;

	const char *content = "another file content";

	int fd = mm_file_open(tmp_path, O_WRONLY | O_TRUNC, 0644);
	test(fd != -1);
	test(mm_file_write_all(fd, content, strlen(content)) == 0);
	test(mm_file_fsync(fd) == 0);
	test(mm_file_close(fd) == 0);

	char *buf;
	size_t size;
	test(mm_file_read_all(tmp_path, &buf, &size) == 0);
	test(size == strlen(content));
	test(memcmp(buf, content, size) == 0);
	mm_free(buf);
}

static void test_write_to_closed_fd(void *arg)
{
	(void)arg;

	int fd = mm_file_open(tmp_path, O_WRONLY | O_TRUNC, 0644);
	test(fd != -1);
	test(mm_file_close(fd) == 0);

	test(mm_file_write_all(fd, "data", 4) == -1);
	test(errno == EBADF);
}

static void test_rename_unlink(void *arg)
{
	(void)arg;

	char new_path[160];
	mm_snprintf(new_path, sizeof(new_path), "%s.renamed", tmp_path);
	unlink(new_path);

	int fd = mm_file_open(tmp_path, O_WRONLY | O_TRUNC, 0644);
	test(fd != -1);
	test(mm_file_write_all(fd, "rename me", 9) == 0);
	test(mm_file_close(fd) == 0);

	test(mm_file_rename(tmp_path, new_path) == 0);
	/* the original file must be gone */
	char *buf;
	size_t size;
	test(mm_file_read_all(tmp_path, &buf, &size) == -1);
	test(errno == ENOENT);

	/* and the new one must exist */
	test(mm_file_read_all(new_path, &buf, &size) == 0);
	test(size == 9);
	mm_free(buf);

	test(mm_file_unlink(new_path) == 0);
	test(mm_file_read_all(new_path, &buf, &size) == -1);
	test(errno == ENOENT);
}

static void test_chmod(void *arg)
{
	(void)arg;

	int fd = mm_file_open(tmp_path, O_WRONLY | O_TRUNC, 0644);
	test(fd != -1);
	test(mm_file_write_all(fd, "chmod", 5) == 0);
	test(mm_file_close(fd) == 0);

	test(mm_file_chmod(tmp_path, 0600) == 0);
	struct stat st;
	test(stat(tmp_path, &st) == 0);
	test((st.st_mode & 0777) == 0600);
}

static void test_open_dir(void *arg)
{
	(void)arg;

	int fd = mm_file_open_dir("/tmp");
	test(fd != -1);
	test(mm_file_close(fd) == 0);

	test(mm_file_open_dir(tmp_path) == -1);
	test(errno == ENOTDIR);
}

static void test_file_coroutine(void *arg)
{
	(void)arg;

	tmp_path_init();

	test_read_all(NULL);
	test_read_all_missing(NULL);
	test_open_write_read_cycle(NULL);
	test_write_to_closed_fd(NULL);
	test_chmod(NULL);
	test_open_dir(NULL);
	/* this test destroys tmp_path, so it must be the last one */
	test_rename_unlink(NULL);

	tmp_path_free();
}

/* the pool is not running: everything must fall back to plain calls */
static void test_file_sync_fallback(void *arg)
{
	(void)arg;

	tmp_path_init();

	const char *content = "sync fallback content";

	int fd = mm_file_open(tmp_path, O_WRONLY | O_TRUNC, 0644);
	test(fd != -1);
	test(mm_file_write_all(fd, content, strlen(content)) == 0);
	test(mm_file_close(fd) == 0);

	char *buf;
	size_t size;
	test(mm_file_read_all(tmp_path, &buf, &size) == 0);
	test(size == strlen(content));
	test(memcmp(buf, content, size) == 0);
	mm_free(buf);

	tmp_path_free();
}

void odyssey_test_file_async(void)
{
	/*
	 * sync fallback: no pool (size 0), no machinarium machines yet,
	 * so we can run in the plain main thread
	 */
	machinarium_set_file_pool_size(0);
	test(machinarium_init() == 0);
	test_file_sync_fallback(NULL);
	machinarium_free();

	/* async: the pool is running, the calls must be done in a machine */
	machinarium_set_file_pool_size(2);
	test(machinarium_init() == 0);

	int64_t rc =
		machine_create("test_file_async", test_file_coroutine, NULL);
	test(rc > 0);
	test(machine_wait(rc) == 0);

	machinarium_free();
}

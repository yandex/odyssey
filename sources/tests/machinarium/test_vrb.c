#include <machinarium/machinarium.h>
#include <machinarium/ds/vrb.h>
#include <tests/odyssey_test.h>

size_t pagesz = 0;

static void test_create_destroy(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(1024);
	test(vrb != NULL);
	test(mm_virtual_rbuf_size(vrb) == 0);
	test(mm_virtual_rbuf_free_size(vrb) == pagesz);
	test(mm_virtual_rbuf_capacity(vrb) == pagesz);

	mm_virtual_rbuf_free(vrb);
}

static void test_create_zero_size(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(0);
	test(vrb != NULL);
	test(mm_virtual_rbuf_size(vrb) == 0);
	test(mm_virtual_rbuf_free_size(vrb) == pagesz);
	test(mm_virtual_rbuf_capacity(vrb) == pagesz);

	mm_virtual_rbuf_free(vrb);
}

static void test_simple_write_read(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(0);
	test(vrb != NULL);
	test(mm_virtual_rbuf_capacity(vrb) == pagesz);

	const char *msg = "AAAAAAAABBBBBBBB";
	size_t msg_len = strlen(msg);
	char buff[128];

	/* lets make write-read cycle go out of bound of 0..capacity */
	size_t n = (mm_virtual_rbuf_capacity(vrb) / msg_len) * 10;

	for (size_t i = 0; i < n; ++i) {
		test(mm_virtual_rbuf_write(vrb, msg, msg_len) == msg_len);
		test(mm_virtual_rbuf_size(vrb) == msg_len);
		test(mm_virtual_rbuf_free_size(vrb) ==
		     mm_virtual_rbuf_capacity(vrb) - msg_len);

		memset(buff, 0, msg_len);
		test(mm_virtual_rbuf_read(vrb, buff, msg_len) == msg_len);
		test(memcmp(buff, msg, msg_len) == 0);
	}

	test(mm_virtual_rbuf_free_size(vrb) == mm_virtual_rbuf_capacity(vrb));
	test(mm_virtual_rbuf_size(vrb) == 0);

	mm_virtual_rbuf_free(vrb);
}

static void test_writes_read(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(4096);

	const char *msg1 = "First";
	const char *msg2 = "Second";
	const char *msg3 = "Third";

	mm_virtual_rbuf_write(vrb, msg1, strlen(msg1));
	mm_virtual_rbuf_write(vrb, msg2, strlen(msg2));
	mm_virtual_rbuf_write(vrb, msg3, strlen(msg3));

	size_t total = strlen(msg1) + strlen(msg2) + strlen(msg3);
	test(mm_virtual_rbuf_size(vrb) == total);

	char buf[100] = { 0 };
	size_t nread = mm_virtual_rbuf_read(vrb, buf, total);
	test(nread == total);
	test(strncmp(buf, "FirstSecondThird", total) == 0);

	mm_virtual_rbuf_free(vrb);
}

static void test_write_read_partial(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(4096);

	const char *msg = "0123456789";
	mm_virtual_rbuf_write(vrb, msg, 10);

	char buf[5] = { 0 };
	size_t nread = mm_virtual_rbuf_read(vrb, buf, 5);
	test(nread == 5);
	test(strncmp(buf, "01234", 5) == 0);
	test(mm_virtual_rbuf_size(vrb) == 5);

	nread = mm_virtual_rbuf_read(vrb, buf, 5);
	test(nread == 5);
	test(strncmp(buf, "56789", 5) == 0);
	test(mm_virtual_rbuf_size(vrb) == 0);

	mm_virtual_rbuf_free(vrb);
}

static void test_write_overflow(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(pagesz);
	test(mm_virtual_rbuf_capacity(vrb) == pagesz);

	size_t data_size = pagesz * 2;
	char *big_data = malloc(data_size);
	memset(big_data, 'A', data_size);

	size_t written = mm_virtual_rbuf_write(vrb, big_data, data_size);
	test(written <= mm_virtual_rbuf_capacity(vrb));
	test(written == mm_virtual_rbuf_capacity(vrb));
	test(mm_virtual_rbuf_free_size(vrb) == 0);

	size_t written2 = mm_virtual_rbuf_write(vrb, "X", 1);
	test(written2 == 0);

	free(big_data);
	mm_virtual_rbuf_free(vrb);
}

static void test_read_empty(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(4096);

	char buf[10];
	size_t nread = mm_virtual_rbuf_read(vrb, buf, 10);
	test(nread == 0);

	mm_virtual_rbuf_free(vrb);
}

static void test_wrap_around_write(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(pagesz);

	size_t data_size = pagesz - 4;
	char *data = malloc(data_size);
	memset(data, 'A', data_size);
	mm_virtual_rbuf_write(vrb, data, data_size);

	size_t nhalf = data_size / 2;
	mm_virtual_rbuf_read(vrb, data, nhalf);
	test(mm_virtual_rbuf_size(vrb) == nhalf);
	test(mm_virtual_rbuf_free_size(vrb) == (pagesz + nhalf - data_size));

	const char *wrap_msg = "WRAP_AROUND_TEST";
	size_t wrap_len = strlen(wrap_msg);
	size_t written = mm_virtual_rbuf_write(vrb, wrap_msg, wrap_len);
	test(written == wrap_len);

	mm_virtual_rbuf_read(vrb, data, nhalf);

	char buf[100] = { 0 };
	size_t nread = mm_virtual_rbuf_read(vrb, buf, wrap_len);
	test(nread == wrap_len);
	test(strcmp(buf, wrap_msg) == 0);

	free(data);

	mm_virtual_rbuf_free(vrb);
}

static void test_drain(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(4096);

	const char *msg = "Test drain functionality";
	size_t msg_len = strlen(msg);
	mm_virtual_rbuf_write(vrb, msg, msg_len);

	size_t drained = mm_virtual_rbuf_drain(vrb, msg_len / 2);
	test(drained == msg_len / 2);
	test(mm_virtual_rbuf_size(vrb) == msg_len - msg_len / 2);

	drained = mm_virtual_rbuf_drain(vrb, 1000);
	test(drained == msg_len - msg_len / 2);
	test(mm_virtual_rbuf_size(vrb) == 0);

	mm_virtual_rbuf_free(vrb);
}

static void test_drain_empty(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(4096);

	size_t drained = mm_virtual_rbuf_drain(vrb, 100);
	test(drained == 0);

	mm_virtual_rbuf_free(vrb);
}

static void test_write_begin_commit(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(4096);

	struct iovec vec = mm_virtual_rbuf_write_begin(vrb);
	test(vec.iov_base != NULL);
	test(vec.iov_len == mm_virtual_rbuf_capacity(vrb));

	const char *msg = "Direct write test";
	size_t msg_len = strlen(msg);
	memcpy(vec.iov_base, msg, msg_len);

	mm_virtual_rbuf_write_commit(vrb, msg_len);
	test(mm_virtual_rbuf_size(vrb) == msg_len);

	char buf[100] = { 0 };
	mm_virtual_rbuf_read(vrb, buf, msg_len);
	test(strcmp(buf, msg) == 0);

	mm_virtual_rbuf_free(vrb);
}

static void test_read_begin_commit(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(pagesz);

	const char *msg = "Direct read test";
	size_t msg_len = strlen(msg);
	mm_virtual_rbuf_write(vrb, msg, msg_len);

	struct iovec vec = mm_virtual_rbuf_read_begin(vrb);
	test(vec.iov_base != NULL);
	test(vec.iov_len == msg_len);

	test(memcmp(vec.iov_base, msg, msg_len) == 0);

	mm_virtual_rbuf_read_commit(vrb, msg_len);
	test(mm_virtual_rbuf_size(vrb) == 0);

	mm_virtual_rbuf_free(vrb);
}

static void test_begin_commit_across_boundary(void)
{
	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(pagesz);

	size_t filler_size = pagesz - 6;
	char *filler = malloc(filler_size);
	memset(filler, 'F', filler_size);
	mm_virtual_rbuf_write(vrb, filler, filler_size);
	mm_virtual_rbuf_drain(vrb, filler_size);

	struct iovec vec = mm_virtual_rbuf_write_begin(vrb);
	const char *msg = "BOUNDARY_TEST_MSG";
	size_t msg_len = strlen(msg);
	memcpy(vec.iov_base, msg, msg_len);
	mm_virtual_rbuf_write_commit(vrb, msg_len);

	vec = mm_virtual_rbuf_read_begin(vrb);
	test(memcmp(vec.iov_base, msg, msg_len) == 0);
	mm_virtual_rbuf_read_commit(vrb, msg_len);

	free(filler);

	mm_virtual_rbuf_free(vrb);
}

void machinarium_test_vrb(void)
{
	machinarium_init();

	pagesz = getpagesize();

	test_create_destroy();
	test_create_zero_size();
	test_simple_write_read();
	test_writes_read();
	test_write_read_partial();
	test_write_overflow();
	test_read_empty();
	test_wrap_around_write();
	test_drain();
	test_drain_empty();
	test_write_begin_commit();
	test_read_begin_commit();
	test_begin_commit_across_boundary();

	machinarium_free();
}

typedef struct {
	char *buffer;
	size_t size;
} plain_memory_t;

static plain_memory_t *plain_memory_create(size_t size)
{
	plain_memory_t *pm = malloc(sizeof(plain_memory_t));
	if (!pm) {
		return NULL;
	}

	pm->size = size;
	pm->buffer = malloc(size);
	if (!pm->buffer) {
		free(pm);
		return NULL;
	}

	return pm;
}

static void plain_memory_free(plain_memory_t *pm)
{
	if (pm) {
		free(pm->buffer);
		free(pm);
	}
}

static size_t plain_memory_write(plain_memory_t *pm, const void *data,
				 size_t len, size_t offset)
{
	if (offset + len > pm->size) {
		len = pm->size - offset;
	}

	memcpy(pm->buffer + offset, data, len);

	return len;
}

#define BUFFER_SIZE (pagesz)
#define ITERATIONS 1000000

static void benchmark_sequential_writes_1KB(void)
{
	printf("\n=== VRB Benchmark: Sequential writes (1KB blocks) ===\n");

	const size_t block_size = 1024;
	char *data = malloc(block_size);
	memset(data, 'A', block_size);

	benchmark_timer_t timer;

	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(BUFFER_SIZE);

	timer_start(&timer);
	for (int i = 0; i < ITERATIONS; i++) {
		if (mm_virtual_rbuf_free_size(vrb) < block_size) {
			mm_virtual_rbuf_drain(vrb, BUFFER_SIZE);
		}
		mm_virtual_rbuf_write(vrb, data, block_size);
	}
	double vrb_time = timer_end(&timer);
	mm_virtual_rbuf_free(vrb);

	plain_memory_t *pm = plain_memory_create(BUFFER_SIZE);
	size_t offset = 0;
	timer_start(&timer);
	for (int i = 0; i < ITERATIONS; i++) {
		if (offset >= (BUFFER_SIZE - block_size)) {
			offset = 0;
		}
		plain_memory_write(pm, data, block_size, offset);
		offset += block_size;
	}
	double plain_time = timer_end(&timer);
	plain_memory_free(pm);

	double vrb_throughput = (ITERATIONS * block_size) / vrb_time / 1e6;
	double plain_throughput = (ITERATIONS * block_size) / plain_time / 1e6;

	printf("Virtual Ring Buffer:  %.6f sec (%.2f MB/s)\n", vrb_time,
	       vrb_throughput);
	printf("Plain Memory:         %.6f sec (%.2f MB/s)\n", plain_time,
	       plain_throughput);
	printf("Overhead:             %.2f%% %s\n",
	       ((vrb_time - plain_time) / plain_time) * 100,
	       vrb_time > plain_time ? "(VRB slower)" : "(VRB faster)");

	free(data);
}

static void benchmark_small_blocks(size_t block_size)
{
	printf("\n=== VRB Benchmark: Small blocks (%lu bytes) ===\n",
	       block_size);

	char data[1024];
	memset(data, 'B', block_size);

	benchmark_timer_t timer;

	mm_virtual_rbuf_t *vrb = mm_virtual_rbuf_create(BUFFER_SIZE);

	timer_start(&timer);
	for (int i = 0; i < ITERATIONS; i++) {
		if (mm_virtual_rbuf_free_size(vrb) < block_size) {
			mm_virtual_rbuf_drain(vrb, BUFFER_SIZE);
		}
		if (mm_virtual_rbuf_write(vrb, data, block_size) !=
		    block_size) {
			abort();
		}
	}
	double vrb_time = timer_end(&timer);
	mm_virtual_rbuf_free(vrb);

	plain_memory_t *pm = plain_memory_create(BUFFER_SIZE);
	size_t offset = 0;

	timer_start(&timer);
	for (int i = 0; i < ITERATIONS; i++) {
		if (offset >= (BUFFER_SIZE - block_size)) {
			offset = 0;
		}
		if (plain_memory_write(pm, data, block_size, offset) !=
		    block_size) {
			abort();
		}
		offset += block_size;
	}
	double plain_time = timer_end(&timer);
	plain_memory_free(pm);

	double vrb_throughput = (ITERATIONS * block_size) / vrb_time / 1e6;
	double plain_throughput = (ITERATIONS * block_size) / plain_time / 1e6;

	printf("Virtual Ring Buffer:  %.6f sec (%.2f MB/s)\n", vrb_time,
	       vrb_throughput);
	printf("Plain Memory:         %.6f sec (%.2f MB/s)\n", plain_time,
	       plain_throughput);
	printf("Overhead:             %.2f%% %s\n",
	       ((vrb_time - plain_time) / plain_time) * 100,
	       vrb_time > plain_time ? "(VRB slower)" : "(VRB faster)");
}

void machinarium_vrb_benchmark(void)
{
	machinarium_init();

	pagesz = getpagesize();

	benchmark_small_blocks(64);
	benchmark_small_blocks(128);
	benchmark_small_blocks(256);
	benchmark_small_blocks(512);

	benchmark_small_blocks(65);
	benchmark_small_blocks(129);
	benchmark_small_blocks(257);
	benchmark_small_blocks(513);

	benchmark_sequential_writes_1KB();

	machinarium_free();
}

#include <odyssey.h>

#include <alloc/linear.h>

#include <tests/odyssey_test.h>

static void test_init(void)
{
	uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	test(al.buf == buf);
	test(al.size == sizeof(buf));
	test(al.used == 0);

	od_linear_alloc_destroy(&al);
}

static void test_alloc_basic(void)
{
	uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));

	void *p = od_linear_alloc_alloc(&al, 16);
	test(p != NULL);
	test(p == buf);
	test(al.used >= 16);

	void *p2 = od_linear_alloc_alloc(&al, 16);
	test(p2 != NULL);
	test(p2 != p);

	od_linear_alloc_destroy(&al);
}

static void test_alloc_zeroed(void)
{
	uint8_t buf[64];
	memset(buf, 0xff, sizeof(buf));

	od_linear_alloc_t al;
	od_linear_alloc_init(&al, buf, sizeof(buf));

	char *p = od_linear_alloc_alloc(&al, 32);
	test(p != NULL);
	for (size_t i = 0; i < 32; i++) {
		test(p[i] == 0);
	}

	od_linear_alloc_destroy(&al);
}

static void test_alloc_overflow(void)
{
	uint8_t buf[16];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));

	void *p = od_linear_alloc_alloc(&al, 16);
	test(p != NULL);

	void *p2 = od_linear_alloc_alloc(&al, 1);
	test(p2 == NULL);

	od_linear_alloc_destroy(&al);
}

static void test_alloc_exact_fit(void)
{
	uint8_t buf[32];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));

	void *p = od_linear_alloc_alloc(&al, sizeof(buf));
	test(p != NULL);
	test(al.used == sizeof(buf));

	void *p2 = od_linear_alloc_alloc(&al, 1);
	test(p2 == NULL);

	od_linear_alloc_destroy(&al);
}

static void test_alloc_alignment(void)
{
	uint8_t buf[256];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));

	void *p = od_linear_alloc_alloc(&al, 1);
	test(p != NULL);
	test(((uintptr_t)p % sizeof(void *)) == 0);

	void *p2 = od_linear_alloc_alloc(&al, 7);
	test(p2 != NULL);
	test(((uintptr_t)p2 % sizeof(void *)) == 0);

	void *p3 = od_linear_alloc_alloc(&al, 13);
	test(p3 != NULL);
	test(((uintptr_t)p3 % sizeof(void *)) == 0);

	od_linear_alloc_destroy(&al);
}

static void test_reset(void)
{
	uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));

	od_linear_alloc_alloc(&al, 32);
	test(al.used > 0);

	od_linear_alloc_reset(&al);
	test(al.used == 0);

	void *p = od_linear_alloc_alloc(&al, sizeof(buf));
	test(p != NULL);
	test(p == buf);

	od_linear_alloc_destroy(&al);
}

static void test_free_is_noop(void)
{
	uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));

	void *p = od_linear_alloc_alloc(&al, 16);
	test(p != NULL);
	size_t used_before = al.used;

	od_linear_alloc_free(&al, p);
	test(al.used == used_before);

	od_linear_alloc_destroy(&al);
}

static void test_many_allocs(void)
{
	uint8_t buf[1024];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));

	int n = 0;
	for (size_t s = 1; s <= 64; s++) {
		void *p = od_linear_alloc_alloc(&al, s);
		if (p == NULL) {
			break;
		}
		n++;
	}
	test(n > 0);

	od_linear_alloc_reset(&al);
	test(al.used == 0);

	od_linear_alloc_destroy(&al);
}

void odyssey_test_linear_alloc(void)
{
	test_init();
	test_alloc_basic();
	test_alloc_zeroed();
	test_alloc_overflow();
	test_alloc_exact_fit();
	test_alloc_alignment();
	test_reset();
	test_free_is_noop();
	test_many_allocs();
}

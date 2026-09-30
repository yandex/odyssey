#include <odyssey.h>

#include <alloc/linear.h>

#include <tests/odyssey_test.h>

static void test_init(void)
{
	_Alignas(max_align_t) uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	test(al.buf == buf);
	test(al.size == sizeof(buf));
	test(al.used == 0);
	test(al.track_sizes == 0);

	od_linear_alloc_destroy(&al);
}

static void test_alloc_basic(void)
{
	_Alignas(max_align_t) uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	/* plain mode has no per-block header */
	void *p = od_linear_alloc_alloc(&al, 16, 0);
	test(p != NULL);
	test(p == buf);
	test(al.used == 16);

	void *p2 = od_linear_alloc_alloc(&al, 16, 0);
	test(p2 != NULL);
	test(p2 != p);
	test(al.used == 32);

	od_linear_alloc_destroy(&al);
}

static void test_alloc_zeroed(void)
{
	_Alignas(max_align_t) uint8_t buf[64];
	memset(buf, 0xff, sizeof(buf));

	od_linear_alloc_t al;
	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	char *p = od_linear_alloc_alloc(&al, 32, OD_LINEAR_ALLOC_ZERO);
	test(p != NULL);
	for (size_t i = 0; i < 32; i++) {
		test(p[i] == 0);
	}

	od_linear_alloc_destroy(&al);
}

static void test_alloc_overflow(void)
{
	_Alignas(max_align_t) uint8_t buf[32];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	void *p = od_linear_alloc_alloc(&al, 16, 0);
	test(p != NULL);

	void *p2 = od_linear_alloc_alloc(&al, 16, 0);
	test(p2 != NULL);
	test(al.used == 32);

	void *p3 = od_linear_alloc_alloc(&al, 1, 0);
	test(p3 == NULL);

	od_linear_alloc_destroy(&al);
}

static void test_alloc_exact_fit(void)
{
	_Alignas(max_align_t) uint8_t buf[32];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	void *p = od_linear_alloc_alloc(&al, sizeof(buf), 0);
	test(p != NULL);
	test(p == buf);
	test(al.used == sizeof(buf));

	void *p2 = od_linear_alloc_alloc(&al, 1, 0);
	test(p2 == NULL);

	od_linear_alloc_destroy(&al);
}

static void test_alloc_alignment(void)
{
	_Alignas(max_align_t) uint8_t buf[256];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	void *p = od_linear_alloc_alloc(&al, 1, 0);
	test(p != NULL);
	test(((uintptr_t)p % alignof(max_align_t)) == 0);

	void *p2 = od_linear_alloc_alloc(&al, 7, 0);
	test(p2 != NULL);
	test(((uintptr_t)p2 % alignof(max_align_t)) == 0);

	void *p3 = od_linear_alloc_alloc(&al, 13, 0);
	test(p3 != NULL);
	test(((uintptr_t)p3 % alignof(max_align_t)) == 0);

	od_linear_alloc_destroy(&al);
}

static void test_mode_switch(void)
{
	_Alignas(max_align_t) uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));

	/* realloc mode pays an alignment header per block */
	od_linear_alloc_reset(&al, OD_LINEAR_ALLOC_REALLOC);
	void *p = od_linear_alloc_alloc(&al, 8, 0);
	test(p != NULL);
	test(p == buf + alignof(max_align_t));
	test(al.used == 2 * alignof(max_align_t));

	/* plain mode after reset does not */
	od_linear_alloc_reset(&al, 0);
	void *p2 = od_linear_alloc_alloc(&al, 8, 0);
	test(p2 != NULL);
	test(p2 == buf);
	test(al.used == alignof(max_align_t));

	od_linear_alloc_destroy(&al);
}

static void test_reset(void)
{
	_Alignas(max_align_t) uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	void *p1 = od_linear_alloc_alloc(&al, 32, 0);
	test(p1 != NULL);
	test(p1 == buf);
	test(al.used == 32);

	od_linear_alloc_reset(&al, 0);
	test(al.used == 0);

	void *p2 = od_linear_alloc_alloc(&al, sizeof(buf), 0);
	test(p2 != NULL);
	test(p2 == buf);

	void *p3 = od_linear_alloc_alloc(&al, 1, 0);
	test(p3 == NULL);

	od_linear_alloc_destroy(&al);
}

static void test_free_is_noop(void)
{
	_Alignas(max_align_t) uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	void *p = od_linear_alloc_alloc(&al, 16, 0);
	test(p != NULL);
	size_t used_before = al.used;

	od_linear_alloc_free(&al, p);
	test(al.used == used_before);

	od_linear_alloc_destroy(&al);
}

static void test_realloc_disabled(void)
{
	_Alignas(max_align_t) uint8_t buf[64];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	void *p = od_linear_alloc_alloc(&al, 16, 0);
	test(p != NULL);

	/* realloc of an existing block requires OD_LINEAR_ALLOC_REALLOC */
	void *p2 = od_linear_alloc_realloc(&al, p, 32);
	test(p2 == NULL);
	test(al.used == 16);

	/* realloc(NULL) is an alloc in any mode */
	void *p3 = od_linear_alloc_realloc(&al, NULL, 16);
	test(p3 != NULL);
	test(al.used == 32);

	od_linear_alloc_destroy(&al);
}

static void test_realloc_mode(void)
{
	_Alignas(max_align_t) uint8_t buf[256];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, OD_LINEAR_ALLOC_REALLOC);

	/* grow the last block in place */
	char *p = od_linear_alloc_alloc(&al, 8, 0);
	test(p != NULL);
	memset(p, 'a', 8);
	test(al.used == 2 * alignof(max_align_t));

	char *g = od_linear_alloc_realloc(&al, p, 32);
	test(g == p);
	test(al.used == alignof(max_align_t) + 32);
	memset(g + 8, 'b', 24);

	/* shrink the last block in place */
	void *p2 = od_linear_alloc_alloc(&al, 8, 0);
	test(p2 != NULL);
	memset(p2, 'c', 8);
	test(al.used == 3 * alignof(max_align_t) + 32);

	char *s = od_linear_alloc_alloc(&al, 32, 0);
	test(s != NULL);

	char *s2 = od_linear_alloc_realloc(&al, s, 16);
	test(s2 == s);
	test(al.used == 4 * alignof(max_align_t) + 48);

	/* realloc to zero frees and returns NULL */
	test(od_linear_alloc_realloc(&al, s2, 0) == NULL);
	test(al.used == 4 * alignof(max_align_t) + 48);

	/* non-last block: reallocated by copy, old block stays in arena */
	char *g2 = od_linear_alloc_realloc(&al, p2, 64);
	test(g2 != NULL);
	test(g2 != p2);
	test(g2[0] == 'c' && g2[7] == 'c');
	test(g[0] == 'a' && g[31] == 'b');
	test(al.used == 5 * alignof(max_align_t) + 112);

	/* exhausted arena: realloc fails and does not modify the arena */
	test(od_linear_alloc_realloc(&al, p, sizeof(buf)) == NULL);
	test(al.used == 5 * alignof(max_align_t) + 112);

	od_linear_alloc_destroy(&al);
}

static void test_realloc_mode_zeroed(void)
{
	_Alignas(max_align_t) uint8_t buf[64];
	memset(buf, 0xff, sizeof(buf));

	od_linear_alloc_t al;
	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, OD_LINEAR_ALLOC_REALLOC);

	char *p = od_linear_alloc_alloc(&al, 16, OD_LINEAR_ALLOC_ZERO);
	test(p != NULL);
	for (size_t i = 0; i < 16; i++) {
		test(p[i] == 0);
	}

	od_linear_alloc_destroy(&al);
}

static void test_many_allocs(void)
{
	_Alignas(max_align_t) uint8_t buf[1024];
	od_linear_alloc_t al;

	od_linear_alloc_init(&al, buf, sizeof(buf));
	od_linear_alloc_reset(&al, 0);

	int n = 0;
	for (size_t s = 1; s <= 64; s++) {
		void *p = od_linear_alloc_alloc(&al, s, 0);
		if (p == NULL) {
			break;
		}
		n++;
	}
	test(n > 0);

	od_linear_alloc_reset(&al, 0);
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
	test_mode_switch();
	test_reset();
	test_free_is_noop();
	test_realloc_disabled();
	test_realloc_mode();
	test_realloc_mode_zeroed();
	test_many_allocs();
}

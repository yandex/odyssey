#include <machinarium/machinarium.h>
#include <machinarium/ds/hm.h>
#include <tests/odyssey_test.h>

static void test_xxh64(void)
{
	test(mm_xxh64_hash("", 0, 0) == 0xef46db3751d8e999);
	test(mm_xxh64_hash("", 0, 1337) == 0x7cefd03d4bb7d3f);
	test(mm_xxh64_hash("a", 1, 15) == 0x4c640756137de265);
	test(mm_xxh64_hash("b", 1, 15) == 0xfdb53a5b73c5c573);
	test(mm_xxh64_hash("c", 1, 15) == 0x99bb220f85650229);
	test(mm_xxh64_hash("\0", 1, 123234) == 0x80fe26b35712a035);
	test(mm_xxh64_hash("ab", 2, 0) == 0x65f708ca92d04a61);
	test(mm_xxh64_hash("\x7\xc", 2, 0) == 0x8bb7ba5b0c790ea0);
	test(mm_xxh64_hash("have you seen my coffee cup?",
			   sizeof("have you seen my coffee cup?") - 1,
			   0) == 0xb9d9779043c0da27);
	test(mm_xxh64_hash("have you seen my coffee cup?",
			   sizeof("have you seen my coffee cup?") - 1,
			   8989898) == 0x48fb8fd6f7af1212);
	const char *text =
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
	test(mm_xxh64_hash(text, strlen(text), 0) == 0xc5a8b11443765630);
	test(mm_xxh64_hash(text, strlen(text), 15) == 0xc984e937d2ca5685);
	test(mm_xxh64_hash("\x61", 1, 0) == 0xd24ec4f1a98c6e5bULL);
	test(mm_xxh64_hash("\x68\x65\x6c\x6c\x6f", 5, 0) ==
	     0x26c7827d889f6da3ULL);
	test(mm_xxh64_hash("\x68\x65\x6c\x6c\x6f\x20\x77\x6f\x72\x6c\x64", 11,
			   0) == 0x45ab6734b21e6968ULL);
	test(mm_xxh64_hash(
		     "\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78",
		     31, 0) == 0x60dd0d01083b99f0ULL);
	test(mm_xxh64_hash(
		     "\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78",
		     32, 0) == 0xe2df261fc2ec30ebULL);
	test(mm_xxh64_hash(
		     "\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78\x78",
		     33, 0) == 0xb3fa465f554208a6ULL);
	test(mm_xxh64_hash("\x68\x65\x6c\x6c\x6f", 5, 3735928559) ==
	     0x0c5badaa04926dcdULL);
	test(mm_xxh64_hash("\x68\x65\x6c\x6c\x6f", 5, 1311768467463790320) ==
	     0xf9d3f0970dab1e3eULL);
	test(mm_xxh64_hash("\x00", 1, 0) == 0xe934a84adb052768ULL);
	test(mm_xxh64_hash("\x00\x01\x02\x03", 4, 0) == 0xffced8604453cc1eULL);
	test(mm_xxh64_hash(
		     "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff",
		     64, 0) == 0x2e54fae2959dbb84ULL);
	test(mm_xxh64_hash("\x00", 1, 123234) == 0x80fe26b35712a035ULL);
}

void machinarium_test_hm_hash(void)
{
	machinarium_init();

	test_xxh64();

	machinarium_free();
}
